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

local SEARCH = "https://music.163.com/api/search/get?s=%s&type=1&limit=30&offset=0"
local OUTER = "https://music.163.com/song/media/outer/url?id=%d"
local HEADERS = {
    ["Referer"] = "https://music.163.com/",
    ["User-Agent"] = "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
                     .. "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 "
                     .. "Mobile/15E148 Safari/604.1",
}
-- 频道 = 搜索关键词。url 转义只处理中文与空格（关键词表内可控）
local CHANNELS = {
    { kw = "华语热门",   name = "华语热门" },
    { kw = "欧美流行",   name = "欧美流行" },
    { kw = "粤语经典",   name = "粤语经典" },
    { kw = "轻音乐",     name = "轻音乐" },
    { kw = "热门翻唱",   name = "热门翻唱" },
}

local channel = 1
local songs = {}             -- { {id,t,s}, ... }
local playing = 0            -- 正在播的下标（0 = 未开始）
local play_id = nil          -- player.play 返回的请求 id
local list_req = nil         -- http 请求 id
local state = "加载中…"      -- 界面状态行
local dirty = true
local fail_streak = 0        -- 连续播放失败（自动跳过坏歌的熔断）

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
    state = "播放中"
    play_id = player.play(string.format(OUTER, song.id))
    refresh()
end

local function next_track()
    if #songs == 0 then return end
    play_idx(playing + 1)
end

-- ---------------------------------------------------------------- 曲库
local function fetch_channel(idx)
    local ch = CHANNELS[idx]
    songs = {}
    playing = 0
    stop_play()
    state = "加载中…"
    list_req = http.get(string.format(SEARCH, urlencode(ch.kw)),
                        { timeout_ms = 10000, max_response_bytes = 32768,
                          headers = HEADERS })
    refresh()
end

local function on_search(body)
    local data = json.decode(body)
    if type(data) ~= "table" then return nil end
    local result = data.result
    if type(result) ~= "table" or type(result.songs) ~= "table" then
        return nil
    end
    local out = {}
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
    return out
end

-- ---------------------------------------------------------------- 绘制
local function render()
    local ch = CHANNELS[channel]
    screen.begin(COL.bg)
    screen.rect(0, 0, W, 26, COL.bar)
    text("电台·" .. ch.name, 10, 5)

    if #songs == 0 then
        text(state, 12, 110, COL.warn)
        text("任意键重试", 12, 140, COL.dim)
    else
        local song = songs[playing]
        if song then
            text(tostring(playing) .. " / " .. #songs, 12, 44, COL.dim)
            text(song.t, 12, 84, COL.hi)
            text(song.s ~= "" and song.s or "未知歌手", 12, 118, COL.text)
            text(state, 12, 152, COL.dim)
        else
            text("待播放", 12, 110, COL.dim)
        end
        text("任意键下一首  左右换频道", 10, H - 52, COL.dim)
    end
    screen.present()
end

-- ---------------------------------------------------------------- 生命周期
function on_start()
    assert(http and json and player, "需要 HTTP/JSON/PLAYER 支持")
    fetch_channel(channel)   -- 启动阶段只登记，安装成功后真正发出
end

function on_http_response(id, response)
    if id ~= list_req then return end
    list_req = nil
    if not response or response.error then
        state = "网络失败"
    elseif response.status ~= 200 then
        state = "HTTP " .. tostring(response.status)
    else
        local out = on_search(response.body)
        if out and #out > 0 then
            songs = out
            fail_streak = 0
            play_idx(1)      -- 曲库到位：自动开播第 1 首
            return
        end
        state = "曲库解析失败"
    end
    refresh()
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
    -- 频道切换
    if id == "left" or id == "right" then
        local d = (id == "right") and 1 or -1
        channel = ((channel - 1 + d) % #CHANNELS + #CHANNELS) % #CHANNELS + 1
        fetch_channel(channel)
        return
    end
    -- 电台语义：任何其它按键 = 下一首
    fail_streak = 0
    next_track()
end

function on_tick()
    if dirty then
        dirty = false
        render()
    end
end
