-- 豆瓣音乐：三个榜单（华语新碟/热门/日韩）浏览 + 专辑详情（简介滚动/朗读）。
-- 接口为移动端 rexxar（匿名可用，需带 Referer 由固件请求头模拟——见下），
-- 榜单与详情都是异步拉取：界面先画占位，回调里再刷。
--
-- 排版预算：每帧 8 段文字 = 标题栏 1 + 内容 5 + 命令条 1 + footer 1。
-- 列表页 4 行条目；详情页 2 段元信息 + 3 段滚动正文。
do
    local C = ui.C

    local API = "https://m.douban.com/rexxar/api/v2"
    -- rexxar 接口匿名可用，但必须带移动端 Referer（实测缺它返回 invalid_request）
    local HEADERS = {
        ["Referer"] = "https://m.douban.com/",
        ["User-Agent"] = "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
                         .. "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 "
                         .. "Mobile/15E148 Safari/604.1",
    }
    local CHARTS = {
        { id = "music_chinese",    name = "华语新碟" },
        { id = "music_hot",        name = "热门音乐" },
        { id = "music_japan_korea", name = "日韩新碟" },
    }
    local ROWS = 4               -- 列表一屏条数
    local D_ROWS = 3             -- 详情一屏正文行数
    local LINE_H, TOP = 30, 40

    local chart = 1              -- 当前榜单下标
    local lists = {}             -- lists[chart.id] = 条目数组 / 状态字符串
    local sel = 1                -- 列表选中下标
    local view = "list"          -- list | detail
    local detail = nil           -- { title, meta, lines, scroll, intro }
    local list_req, det_req = nil, nil
    local det_id = nil           -- 正在拉详情的条目 id（防串台）
    local speech = nil           -- tts 请求 id
    local d_scroll               -- 详情滚动（key 闭包引用，后面定义）

    -- 按像素宽度折行（ASCII 8px、其余 16px），按 UTF-8 字符边界切。
    local function wrap(s, max_px)
        local out, start, x, i = {}, 1, 0, 1
        while i <= #s do
            local b = string.byte(s, i)
            local n, w = 1, 8
            if b >= 0xF0 then n, w = 4, 16
            elseif b >= 0xE0 then n, w = 3, 16
            elseif b >= 0x80 then n, w = 2, 16 end
            if b == 10 then
                out[#out + 1] = string.sub(s, start, i - 1)
                i = i + 1
                start, x = i, 0
            else
                if x + w > max_px and i > start then
                    out[#out + 1] = string.sub(s, start, i - 1)
                    start, x = i, 0
                end
                x = x + w
                i = i + n
            end
        end
        if start <= #s or #out == 0 then out[#out + 1] = string.sub(s, start) end
        return out
    end

    local function join_names(t)
        -- 榜单接口 singer 是字符串数组；详情接口是对象数组（取 name 字段）
        if type(t) ~= "table" or #t == 0 then return "" end
        local out = {}
        for i = 1, #t do
            local v = t[i]
            if type(v) == "string" then
                out[#out + 1] = v
            elseif type(v) == "table" and type(v.name) == "string" then
                out[#out + 1] = v.name
            end
        end
        return table.concat(out, "/")
    end

    local function rating_text(it)
        if type(it.rating) == "table" and it.rating.value then
            return string.format("%.1f", it.rating.value)
        end
        return "暂无"
    end

    local function fetch_list(idx)
        local ch = CHARTS[idx]
        if type(lists[ch.id]) == "table" then return end   -- 已有数据
        lists[ch.id] = "加载中…"
        list_req = http.get(API .. "/subject_collection/" .. ch.id ..
                            "/items?start=0&count=50",
                            { timeout_ms = 10000, max_response_bytes = 32768,
                              headers = HEADERS })
        sel = 1
        ui.refresh()
    end

    local function open_detail()
        local items = lists[CHARTS[chart].id]
        if type(items) ~= "table" then return end
        local it = items[sel]
        if not it then return end
        det_id = it.id
        detail = { title = it.t, meta = it.s .. "  ·  评分 " .. it.r,
                   lines = { "加载中…" }, scroll = 0, intro = nil }
        view = "detail"
        det_req = http.get(API .. "/music/" .. tostring(it.id),
                           { timeout_ms = 10000, max_response_bytes = 32768,
                             headers = HEADERS })
        ui.refresh()
    end

    local function close_detail()
        if speech and ui.HAS_TTS then tts.cancel(speech) end
        speech = nil
        view = "list"
        detail = nil
        det_req = nil
        ui.refresh()
    end

    -- ---------------------------------------------------------------- 绘制
    local function draw_list()
        local ch = CHARTS[chart]
        local items = lists[ch.id]
        ui.begin("豆瓣音乐·" .. ch.name)

        if type(items) ~= "table" then
            -- 状态字符串：加载中 / 错误信息（footer 也用不上，这里居中给足）
            ui.text_center(items or "未加载", 120, C.warn)
        else
            -- 选中行可视窗口
            local top = sel - ROWS + 1
            if top < 1 then top = 1 end
            for i = 1, ROWS do
                local idx = top + i - 1
                local it = items[idx]
                if it then
                    local y = TOP + (i - 1) * LINE_H
                    if idx == sel then
                        screen.rect(0, y - 4, 240, LINE_H, C.bar)
                    end
                    local line = idx .. " " .. it.t .. " " .. it.s .. " " .. it.r
                    ui.text(line, 12, y, idx == sel and C.hi or C.text)
                end
            end
        end

        ui.footer(type(items) == "table"
                  and string.format("%d/%d  左右换榜  A键详情", sel, #items)
                  or "左右换榜  A键刷新", C.dim)
        ui.commands({
            { label = "详情", run = open_detail },
            { label = "换榜单", run = function()
                chart = chart % #CHARTS + 1
                fetch_list(chart)
            end },
            { label = "刷新", run = function()
                lists[CHARTS[chart].id] = nil
                fetch_list(chart)
            end },
        })
    end

    local function draw_detail()
        local d = detail
        ui.begin("豆瓣音乐·详情")

        ui.text(d.title, 12, TOP, C.hi)
        ui.text(d.meta, 12, TOP + 22, C.dim)
        for i = 1, D_ROWS do
            local line = d.lines[d.scroll + i]
            if line then ui.text(line, 12, TOP + 50 + (i - 1) * 22, C.text) end
        end

        local total = #d.lines
        local page = math.floor(d.scroll / D_ROWS) + 1
        local pages = math.max(1, math.ceil(total / D_ROWS))
        ui.footer(string.format("%d/%d  方向键滚动", page, pages), C.dim)
        ui.commands({
            { label = "返回列表", run = close_detail },
            { label = "朗读简介", run = function()
                if not ui.HAS_TTS or not d.intro then return end
                if speech then tts.cancel(speech) end
                -- 简介动辄几千字，截前 480 字节（UTF-8 边界）朗读
                speech = tts.speak(ui.clip(d.intro, 480))
            end },
        })
    end

    -- ---------------------------------------------------------------- http
    local function on_items(body)
        local data = json.decode(body)
        if type(data) ~= "table" or type(data.subject_collection_items) ~= "table" then
            return "返回格式不认识"
        end
        local out = {}
        for i = 1, #data.subject_collection_items do
            local it = data.subject_collection_items[i]
            if type(it) == "table" and type(it.title) == "string" then
                out[#out + 1] = {
                    t = it.title,
                    s = join_names(it.singer),
                    r = rating_text(it),
                    id = it.id,
                }
            end
        end
        if #out == 0 then return "榜单为空" end
        return out
    end

    local function on_detail(body)
        local data = json.decode(body)
        if type(data) ~= "table" or type(data.title) ~= "string" then
            return nil
        end
        local meta = {}
        local singer = join_names(data.singer)
        if singer ~= "" then meta[#meta + 1] = singer end
        if type(data.genres) == "table" and #data.genres > 0 then
            meta[#meta + 1] = table.concat(data.genres, "/")
        end
        if type(data.pubdate) == "table" and data.pubdate[1] then
            meta[#meta + 1] = data.pubdate[1]
        end
        meta[#meta + 1] = "评分 " .. rating_text(data)
        if type(data.comment_count) == "number" then
            meta[#meta + 1] = data.comment_count .. " 人评"
        end
        local intro = type(data.intro) == "string" and data.intro or "暂无简介"
        return { title = data.title, meta = table.concat(meta, " · "),
                 lines = wrap(intro, 216), scroll = 0, intro = intro }
    end

    ui.register({
        id = "douban",
        name = "豆瓣音乐",
        icon = "music",
        color = 0x7E57C2,
        enter = function(self) fetch_list(chart) end,
        tick = function(self, dt) end,
        draw = function(self)
            if view == "detail" and detail then draw_detail() else draw_list() end
        end,
        key = function(self, id)
            if view == "list" then
                local items = lists[CHARTS[chart].id]
                if id == "up" and type(items) == "table" and #items > 0 then
                    sel = (sel - 2) % #items + 1
                    ui.refresh()
                    return true
                elseif id == "down" and type(items) == "table" and #items > 0 then
                    sel = sel % #items + 1
                    ui.refresh()
                    return true
                elseif id == "left" or id == "right" then
                    local d = (id == "right") and 1 or -1
                    chart = ((chart - 1 + d) % #CHARTS + #CHARTS) % #CHARTS + 1
                    fetch_list(chart)
                    return true
                end
            elseif view == "detail" and detail then
                if id == "up" or id == "left" then
                    d_scroll(-D_ROWS)
                    return true
                elseif id == "down" or id == "right" then
                    d_scroll(D_ROWS)
                    return true
                end
            end
            return false
        end,
        http = function(self, id, response)
            if id == list_req then
                list_req = nil
                local ch = CHARTS[chart]
                if not response or response.error then
                    lists[ch.id] = "网络失败"
                elseif response.status ~= 200 then
                    lists[ch.id] = "HTTP " .. tostring(response.status)
                else
                    -- 注意沙箱没有 pcall：decode 抛错即终止实例。响应体上限
                    -- 32KB、实测榜单最大 23KB/详情 18KB，留有余量；仍以
                    -- 结构校验兜底（decode 成功但字段不对走这里的字符串态）。
                    lists[ch.id] = on_items(response.body)
                end
                ui.refresh()
                return
            end
            if id == det_req then
                det_req = nil
                if detail and response and not response.error and response.status == 200 then
                    local d = on_detail(response.body)
                    if d then detail = d end
                end
                if detail and detail.intro == nil then
                    detail.lines = { "加载失败" }
                end
                ui.refresh()
            end
        end,
        tts = function(self, id, result)
            if id == speech then speech = nil end
        end,
        leave = function(self)
            if speech and ui.HAS_TTS then tts.cancel(speech) end
            speech = nil
            -- 在飞的 http 请求不取消：回调按请求 id 过滤，离开后自然作废
        end,
    })

    -- 详情正文滚动（key 闭包引用，定义在注册之后）
    d_scroll = function(delta)
        local max = #detail.lines - D_ROWS
        if max <= 0 then return end
        local s = detail.scroll + delta
        if s < 0 then s = 0 elseif s > max then s = max end
        if s ~= detail.scroll then detail.scroll = s; ui.refresh() end
    end
end
