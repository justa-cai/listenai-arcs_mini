-- 网易云音乐电台（独立小应用）：进入即播，任意键换下一首。
-- 曲库来自网易云搜索接口（匿名 GET，需 Referer；~23KB/30 首），
-- 播放走外链 https://music.163.com/song/media/outer/url?id=<id>（302 到
-- mp3 直链，VIP/下架歌 404 —— 播放失败自动跳下一首）。
-- "播"用本固件 miniapp player API：player.play(url)（app_player 网络流），
-- 播完 on_player_result(status="completed") 自动续下一首。
--
-- 左右方向键换频道（= 换搜索词重新拉曲库），其余任意键下一首。
-- 每帧预算：矩形 ≤128、文字 ≤8 段。
local W, H = 240, 240
local COL = {
    bg   = 0x101923,
    bar  = 0x1C2A3A,
    text = 0xFFFFFF,
    hi   = 0x7E9CFF,
    dim  = 0xA5B5C5,
    warn = 0xFFB84D,
}

local SEARCH = "https://music.163.com/api/search/get?s=%s&type=1&limit=30&offset=%d"
local NEWSONG = "https://music.163.com/api/personalized/newsong?limit=6"
local DETAIL = "https://music.163.com/api/song/detail?ids=%%5B%d%%5D"
local OUTER = "https://music.163.com/song/media/outer/url?id=%d"
local HEADERS = {
    ["Referer"] = "https://music.163.com/",
    ["User-Agent"] = "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
                     .. "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 "
                     .. "Mobile/15E148 Safari/604.1",
}
-- 频道三类来源：kind="roam" 随机漫游（默认"为你推荐"：随机风格词 +
-- 随机搜索页拉一批，播完再漫游下一批，配已播去重——匿名 newsong
-- 是固定快照（实测两次调用 6 首完全相同），靠它循环等于 6 首打转）；
-- kind="newsong" 新歌速递（固定一批，循环）；kind="search" 固定关键词
-- + offset 翻页追加。
local ROAM_WORDS = {
    "流行", "民谣", "摇滚", "电子", "爵士", "嘻哈", "乡村", "古风",
    "治愈", "清晨", "夜晚", "旅行", "咖啡馆", "运动", "雨天", "怀旧",
    "热门单曲", "经典老歌", "网络热歌", "翻唱",
}

local CHANNELS = {
    { kind = "roam",    name = "为你推荐" },
    { kind = "newsong", name = "新歌速递" },
    { kind = "search", kw = "华语热门",   name = "华语热门" },
    { kind = "search", kw = "欧美流行",   name = "欧美流行" },
    { kind = "search", kw = "粤语经典",   name = "粤语经典" },
    { kind = "search", kw = "轻音乐",     name = "轻音乐" },
}

local channel = 1
local songs = {}             -- { {id,t,s,pic}, ... }（pic 为可带封面直链）
local playing = 0            -- 正在播的下标（0 = 未开始）
local play_id = nil          -- player.play 返回的请求 id
local list_req = nil         -- http 请求 id
local state = "加载中…"      -- 界面状态行
local dirty = true
local fail_streak = 0        -- 连续播放失败（自动跳过坏歌的熔断）
local paused = false         -- 双击暂停标志
local offset = 0             -- 搜索频道已拉到的 offset
local fetch_more             -- 前向声明：next_track 引用（曾漏声明，
                             -- 第三首切第四首触发翻页时 nil 调用崩溃）
local played = {}            -- 已播歌曲去重（id -> true，容量见 PLAYED_MAX）
local played_count = 0
local PLAYED_MAX = 240
local roam_word = nil        -- 当前漫游批次的搜索词（界面展示用）
local roam_tries = 0         -- 漫游批次"全为已播"的重拉次数（防死循环）

-- 伪随机：clock.now() 除以质数步长取整再取模（除法结果是浮点，
-- math.tointeger 会返回 nil —— 曾因此崩溃，必须 math.floor）
local function rand_below(n, salt)
    return math.floor(clock.now() / (700 + salt * 211)) % n
end

-- 封面：song/detail 拿 picUrl → CDN ?param=180y180 缩放 → 下载 jpeg
local cover = nil            -- {jpeg=string} 当前已下载封面（歌曲切换时作废）
local cover_song = nil       -- 封面对应的歌曲 id
local cover_req = nil        -- 封面下载请求 id
local detail_req = nil       -- 详情（picUrl）请求 id
local COVER_SIZE = 100       -- 界面上封面区域（左侧竖版布局）

local function clip(s, limit)
    s = tostring(s)
    if #s <= limit then return s end
    local cut = limit
    while cut > 0 do
        local b = string.byte(s, cut + 1)
        if b and b >= 0x80 and b < 0xC0 then cut = cut - 1 else break end
    end
    return string.sub(s, 1, cut)
end

local function text(s, x, y, c)
    screen.text(clip(s, 63), x, y, c or COL.text)
end

local function refresh()
    dirty = true
end

local function urlencode(s)
    local out = {}
    for i = 1, #s do
        local b = string.byte(s, i)
        if b >= 0x80 then
            out[#out + 1] = string.format("%%%02X", b)
        elseif b == 32 then
            out[#out + 1] = "%20"
        else
            out[#out + 1] = string.sub(s, i, i)
        end
    end
    return table.concat(out)
end

-- ---------------------------------------------------------------- 播放
local function stop_play()
    if play_id and player then player.stop() end
    play_id = nil
end

local function play_idx(idx)
    if #songs == 0 then return end
    if idx > #songs then idx = 1 end          -- 列表尾循环
    playing = idx
    local song = songs[idx]
    stop_play()
    paused = false
    -- 封面：推荐频道自带 picUrl 直下；搜索频道 song/detail 换取
    cover = nil
    cover_song = song.id
    if cover_req then http.cancel(cover_req); cover_req = nil end
    if detail_req then http.cancel(detail_req); detail_req = nil end
    if type(song.pic) == "string" and #song.pic > 0 then
        cover_req = http.get(song.pic .. "?param=100y100",
                             { timeout_ms = 10000, max_response_bytes = 32768,
                               headers = HEADERS })
    else
        detail_req = http.get(string.format(DETAIL, song.id),
                              { timeout_ms = 8000, max_response_bytes = 8192,
                                headers = HEADERS })
    end
    if not played[song.id] then
        played[song.id] = true
        played_count = played_count + 1
        if played_count > PLAYED_MAX then
            played = {}           -- 容量到顶整体清空：长听时允许自然重听
            played_count = 0
        end
    end
    state = "播放中"
    play_id = player.play(string.format(OUTER, song.id))
    refresh()
end

local function next_track()
    if #songs == 0 then return end
    -- 曲库将尽：提前翻页/续批（推荐永远新批；搜索 offset 未到底时追加）
    if playing >= #songs - 2 and not list_req then
        local ch = CHANNELS[channel]
        if ch.kind == "roam" or ch.kind == "newsong" or offset >= 0 then
            fetch_more()
        end
    end
    if playing >= #songs and offset < 0 then
        play_idx(1)      -- 搜索到底/newsong 固定批：回开头循环
        return
    end
    -- 已播去重：跳到下一首没播过的
    local idx = playing + 1
    while idx <= #songs and played[songs[idx].id] do
        idx = idx + 1
    end
    if idx > #songs then
        -- 本批全部播过：漫游频道立刻换批（限次防死循环）
        local ch = CHANNELS[channel]
        if ch.kind == "roam" and roam_tries < 3 and not list_req then
            roam_tries = roam_tries + 1
            fetch_more()
        else
            play_idx(1)
        end
        return
    end
    play_idx(idx)
end

-- ---------------------------------------------------------------- 曲库
local function fetch_channel(idx)
    local ch = CHANNELS[idx]
    songs = {}
    playing = 0
    paused = false
    offset = 0
    stop_play()
    state = "加载中…"
    if ch.kind == "roam" then
        roam_tries = 0
        roam_word = ROAM_WORDS[rand_below(#ROAM_WORDS, 1) + 1]
        list_req = http.get(string.format(SEARCH, urlencode(roam_word),
                                          rand_below(4, 2) * 30),
                            { timeout_ms = 10000, max_response_bytes = 32768,
                              headers = HEADERS })
    elseif ch.kind == "newsong" then
        list_req = http.get(NEWSONG,
                            { timeout_ms = 10000, max_response_bytes = 32768,
                              headers = HEADERS })
    else
        list_req = http.get(string.format(SEARCH, urlencode(ch.kw), 0),
                            { timeout_ms = 10000, max_response_bytes = 32768,
                              headers = HEADERS })
    end
    refresh()
end

-- 拉更多：搜索频道翻页追加；推荐频道永远拉新一批
fetch_more = function()
    local ch = CHANNELS[channel]
    if ch.kind == "roam" then
        roam_tries = 0
        roam_word = ROAM_WORDS[rand_below(#ROAM_WORDS, 3) + 1]
        list_req = http.get(string.format(SEARCH, urlencode(roam_word),
                                          rand_below(4, 4) * 30),
                            { timeout_ms = 10000, max_response_bytes = 32768,
                            headers = HEADERS })
    elseif ch.kind == "newsong" then
        -- 固定快照：不续批，回到本批开头循环
        list_req = nil
        return
    else
        offset = #songs
        list_req = http.get(string.format(SEARCH, urlencode(ch.kw), offset),
                            { timeout_ms = 10000, max_response_bytes = 32768,
                              headers = HEADERS })
    end
end

local function on_search(body)
    local data = json.decode(body)
    if type(data) ~= "table" then return nil end
    local result = data.result
    if type(result) ~= "table" then return nil end
    local out = {}
    if type(result.songs) == "table" then
        -- 搜索：songs[] {id,name,artists[]}
        for i = 1, #result.songs do
            local s = result.songs[i]
            if type(s) == "table" and type(s.id) == "number" and
               type(s.name) == "string" then
                local artist = ""
                if type(s.artists) == "table" and type(s.artists[1]) == "table" and
                   type(s.artists[1].name) == "string" then
                    artist = s.artists[1].name
                end
                out[#out + 1] = { id = s.id, t = s.name, s = artist }
            end
        end
    elseif type(result) == "table" and type(result[1]) == "table" and
           type(result[1].song) == "table" then
        -- 推荐 newsong：result[] {song:{id,name,artists[],album.picUrl}, picUrl}
        for i = 1, #result do
            local s = result[i].song
            if type(s.id) == "number" and type(s.name) == "string" then
                local artist = ""
                if type(s.artists) == "table" and type(s.artists[1]) == "table" and
                   type(s.artists[1].name) == "string" then
                    artist = s.artists[1].name
                end
                local pic = type(result[i].picUrl) == "string" and
                            result[i].picUrl or nil
                out[#out + 1] = { id = s.id, t = s.name, s = artist, pic = pic }
            end
        end
    end
    return out
end

-- ---------------------------------------------------------------- 绘制
local DISC_CX, DISC_CY, DISC_R = 120, 88, 46   -- 唱片转盘几何
local spin = 0                                  -- 转盘角度（每 tick 递增）
local last_spin_ms = 0

local function fmt_ms(ms)
    local s = math.floor(ms / 1000)
    return string.format("%d:%02d", math.floor(s / 60), s % 60)
end

local function draw_disc(playing_now)
    -- 黑胶转盘：三层同心方块 + 中心轴 + 绕圈高光点（近似圆形视觉）
    screen.rect(DISC_CX - DISC_R, DISC_CY - DISC_R, DISC_R * 2, DISC_R * 2, COL.bar)
    screen.rect(DISC_CX - DISC_R + 8, DISC_CY - DISC_R + 8,
                (DISC_R - 8) * 2, (DISC_R - 8) * 2, 0x0D1520)
    screen.rect(DISC_CX - 12, DISC_CY - 12, 24, 24, COL.hi)
    -- 高光点绕中心旋转（预计算 16 相位坐标，避免 tick 里算三角）
    local phase = (spin % 16) + 1
    local QUADS = {
        {1, 0}, {2, 0}, {3, 1}, {3, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 3},
        {0, 2}, {0, 1}, {0, 0}, {1, 0}, {2, 0}, {3, 1}, {3, 2}, {2, 3},
    }
    local q = QUADS[phase]
    local rr = DISC_R - 16
    local hx = DISC_CX - 4 + math.floor((q[1] - 1.5) / 1.5 * rr)
    local hy = DISC_CY - 4 + math.floor((q[2] - 1.5) / 1.5 * rr)
    screen.rect(hx, hy, 8, 8, playing_now and 0xFFFFFF or COL.dim)
end

local function render()
    local ch = CHANNELS[channel]
    screen.begin(COL.bg)
    screen.rect(0, 0, W, 26, COL.bar)
    text("♪ " .. (ch.kind == "roam" and (roam_word and (ch.name .. "·" .. roam_word) or ch.name) or ch.name), 10, 5)

    if #songs == 0 then
        text(state, 12, 110, COL.warn)
        text("任意键重试", 12, 140, COL.dim)
        screen.present()
        return
    end

    local song = songs[playing]
    if not song then
        text("待播放", 12, 110, COL.dim)
        screen.present()
        return
    end

    local pos, dur = 0, 0
    if play_id then
        pos = player.position()
        dur = player.duration()
    end
    text(tostring(playing) .. "/" .. #songs, W - 56, 5, COL.dim)

    -- 布局：左侧封面 100x100（转盘占位同区域），右侧信息栏；底部进度区
    local cover_ok = false
    if cover and cover.jpeg then
        cover_ok = screen.image(12, 44, cover.jpeg)
    end
    if not cover_ok then
        -- 转盘占位：中心 (62,94) 半径 42
        local saved_cx, saved_cy, saved_r = DISC_CX, DISC_CY, DISC_R
        DISC_CX, DISC_CY, DISC_R = 62, 94, 42
        draw_disc(play_id ~= nil and state == "播放中")
        DISC_CX, DISC_CY, DISC_R = saved_cx, saved_cy, saved_r
    end

    -- 右列 x=124 起（宽 104 ≈ 13 汉字）
    text(song.t, 124, 48, COL.hi)
    text(song.s ~= "" and song.s or "未知歌手", 124, 76, COL.text)
    text(state, 124, 104, COL.dim)

    -- 底部进度区：整宽进度条 + 时间/提示行
    screen.rect(12, 170, 216, 6, COL.bar)
    if dur > 0 and pos <= dur then
        local w = math.floor(216 * pos / dur)
        if w > 0 then screen.rect(12, 169, w, 8, COL.hi) end
    end
    text(pos > 0 and fmt_ms(pos) .. "/" .. fmt_ms(dur) or "--:--/--:--", 12, 188, COL.dim)
    text("任意键下一首 左右换台", 12, 214, COL.dim)
    screen.present()
end

-- ---------------------------------------------------------------- 生命周期
function on_start()
    assert(http and json and player, "需要 HTTP/JSON/PLAYER 支持")
    fetch_channel(channel)   -- 启动阶段只登记，安装成功后真正发出
end

function on_http_response(id, response)
    if id == list_req then
        local appending = #songs > 0          -- 翻页/续批（非首拉）
        list_req = nil
        if not response or response.error then
            state = appending and "续拉失败" or "网络失败"
        elseif response.status ~= 200 then
            state = "HTTP " .. tostring(response.status)
        else
            local out = on_search(response.body)
            if out and #out > 0 then
                -- 漫游批次：过滤掉已播过的（其余频道保留全量，循环听合理）
                if CHANNELS[channel].kind == "roam" then
                    local fresh = {}
                    for i = 1, #out do
                        if not played[out[i].id] then fresh[#fresh + 1] = out[i] end
                    end
                    out = fresh
                end
                if #out > 0 then
                if appending then
                    for i = 1, #out do songs[#songs + 1] = out[i] end
                    -- 追加批次不立即播：等当前首播完自然走到新歌
                    refresh()
                    return
                end
                songs = out
                fail_streak = 0
                play_idx(1)      -- 曲库到位：自动开播第 1 首
                return
                end
            end
            if appending then
                -- 翻页拉空（搜索到底）：标记不再翻，回开头循环
                offset = -1
                refresh()
                return
            end
            state = "曲库解析失败"
        end
        refresh()
        return
    end
    -- 详情：提取 picUrl → 下载 180x180 封面（CDN param 缩放）
    if id == detail_req then
        detail_req = nil
        if cover_song and response and not response.error and response.status == 200 then
            local d = json.decode(response.body)
            local songs_arr = type(d) == "table" and d.songs or nil
            local pic = songs_arr and songs_arr[1] and songs_arr[1].album or nil
            pic = pic and pic.picUrl or nil
            if type(pic) == "string" and #pic > 0 then
                cover_req = http.get(pic .. "?param=100y100",
                                     { timeout_ms = 10000,
                                       max_response_bytes = 32768,
                                       headers = HEADERS })
            end
        end
        return
    end
    -- 封面字节到达（二进制 body）
    if id == cover_req then
        cover_req = nil
        if cover_song and response and not response.error and
           response.status == 200 and type(response.body) == "string" and
           #response.body > 100 then
            cover = { jpeg = response.body }
            refresh()
        end
    end
end

function on_player_result(id, result)
    if id ~= play_id then return end
    play_id = nil
    local status = type(result) == "table" and result.status or tostring(result)
    if status == "completed" then
        fail_streak = 0
        next_track()                        -- 播完自动下一首
    elseif status == "stopped" then
        state = "已停"                      -- 主动 stop（按键/切频道），不自动续
        refresh()
    else                                    -- failed：404/VIP/网络 —— 跳下一首
        fail_streak = fail_streak + 1
        if fail_streak >= 10 then           -- 熔断：整库都是坏歌时别死循环
            state = "连续失败 " .. fail_streak .. " 次"
            fail_streak = 0
            refresh()
            return
        end
        next_track()
    end
end

function on_button_click(id)
    -- 加载/失败态：任何键重拉
    if #songs == 0 then
        fetch_channel(channel)
        return
    end
    -- 双击 = 暂停/恢复
    if id == "function_double" then
        if play_id then
            if paused then
                if player.resume() then paused = false; state = "播放中" end
            else
                if player.pause() then paused = true; state = "已暂停" end
            end
            refresh()
            return
        end
    end
    -- 频道切换
    if id == "left" or id == "right" then
        local d = (id == "right") and 1 or -1
        channel = ((channel - 1 + d) % #CHANNELS + #CHANNELS) % #CHANNELS + 1
        fetch_channel(channel)
        return
    end
    -- 电台语义：任何其它按键 = 下一首（暂停状态按键也切歌并恢复播放）
    fail_streak = 0
    next_track()
end

function on_tick()
    -- 播放中：转盘旋转 + 进度条走动，按 ~150ms 节拍重绘（约每 2.4 相位一格）
    if play_id then
        local now = clock.now()
        if now - last_spin_ms >= 150 then
            last_spin_ms = now
            spin = spin + 1
            dirty = true
        end
    end
    if dirty then
        dirty = false
        render()
    end
end
