-- 一言：联网取一句随机句子（https://v1.hitokoto.cn/，无需密钥）。
-- HTTP 是异步的：先画界面再等结果，并把传输错误 / HTTP 状态 / 业务字段分开判断。
-- 启动阶段不发请求（运行环境会把它登记到安装成功后再发），只靠命令触发刷新。
--
-- 排版：句子长度不可控，所以按"像素宽度"折行（一行 13 个汉字 ≈ 216px，ASCII 减半），
-- 一屏放 4 行，超出的部分滚动查看（右侧有滚动条）。滚动两种入口都给了：
-- 手柄方向键直接翻（壳层的 key 钩子），没有手柄时用命令条里的"翻页"。
do
    local C = ui.C

    local URL = "https://v1.hitokoto.cn/"
    local VISIBLE = 4                  -- 一屏行数（文字额度见下）
    local WRAP_PX = 216                -- 折行宽度：12..228，右侧 233 留给滚动条
    local LINE_H, TOP = 30, 44
    local BAR_X, BAR_Y, BAR_W, BAR_H = 233, 42, 3, 110

    local req_id, state, lines, scroll = nil, "按「换一句」获取", {}, 0

    -- 按"像素宽度"折行：ASCII 8px、其余 16px。必须按字符边界切，切半个 UTF-8
    -- 字符设备会直接报错。长句子不再截断 —— 靠滚动看完。
    local function wrap(s, max_px)
        local out, start, x, i = {}, 1, 0, 1
        while i <= #s do
            local b = string.byte(s, i)
            local n = 1
            local w = 8
            if b >= 0xF0 then n, w = 4, 16
            elseif b >= 0xE0 then n, w = 3, 16
            elseif b >= 0x80 then n, w = 2, 16 end

            if b == 10 then                                  -- 原文里的换行
                out[#out + 1] = string.sub(s, start, i - 1)
                i = i + 1
                start, x = i, 0
            else
                if x + w > max_px and i > start then         -- 放不下就断行
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

    local function max_scroll()
        local m = #lines - VISIBLE
        return m > 0 and m or 0
    end

    local function page()
        local total = math.ceil(#lines / VISIBLE)
        return math.floor(scroll / VISIBLE) + 1, total > 0 and total or 1
    end

    -- 返回 true 表示这个按键被滚动用掉了（壳层则不再走命令条）
    local function scroll_by(d)
        local m = max_scroll()
        if m == 0 then return false end
        local s = scroll + d
        if s < 0 then s = 0 elseif s > m then s = m end
        if s ~= scroll then scroll = s; ui.refresh() end
        return true
    end

    local function refresh()
        if not ui.HAS_HTTP then state = "本机无网络接口"; ui.refresh(); return end
        req_id = http.get(URL, { timeout_ms = 8000, max_response_bytes = 2048 })
        state = "请求中…"
        ui.refresh()
    end

    local function draw()
        ui.begin("一言")

        for i = 1, VISIBLE do
            local line = lines[scroll + i]
            if line then ui.text(line, 12, TOP + (i - 1) * LINE_H, C.text) end
        end

        -- 滚动条：只在内容超过一屏时出现，位置按 scroll 比例算
        if #lines > VISIBLE then
            screen.rect(BAR_X, BAR_Y, BAR_W, BAR_H, C.line)
            local th = math.max(12, math.floor(BAR_H * VISIBLE / #lines))
            local ty = BAR_Y + math.floor((BAR_H - th) * scroll / max_scroll())
            screen.rect(BAR_X, ty, BAR_W, th, C.hi)
        end

        -- 一行文字兼作状态行：出错/请求中优先，否则给翻页进度与操作提示
        if state ~= "" and state ~= "已更新" then
            ui.footer(state, C.warn)
        elseif #lines > VISIBLE then
            local p, t = page()
            ui.footer(string.format("第 %d/%d 页  方向键翻", p, t), C.dim)
        elseif #lines > 0 then
            ui.footer("方向键翻  A键换一句", C.dim)
        else
            ui.footer(" ", C.dim)
        end

        -- 命令条：没有手柄时靠它翻页/换句（命令条本身也在教用户有哪些操作）
        local cmds = { { label = "返回", run = ui.back },
                       { label = "换一句", run = refresh } }
        if #lines > VISIBLE then
            cmds[#cmds + 1] = { label = "翻页", run = function()
                if not scroll_by(VISIBLE) or scroll + VISIBLE > max_scroll() then
                    scroll = 0        -- 到底了再按就回到开头
                    ui.refresh()
                end
            end }
        end
        ui.commands(cmds)
    end

    ui.register({
        id = "quote",
        name = "一言",
        icon = "quote",
        color = C.warn,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        -- 手柄方向键直接滚动（命令条还在，所以两种输入都能操作）
        key = function(self, id)
            if id == "up" then return scroll_by(-1) end
            if id == "down" then return scroll_by(1) end
            if id == "left" then return scroll_by(-VISIBLE) end
            if id == "right" then return scroll_by(VISIBLE) end
            return false
        end,
        http = function(self, id, response)
            if id ~= req_id then return end
            req_id = nil
            if not response then state = "无响应"; ui.refresh(); return end
            if response.error then
                state = "网络失败: " .. tostring(response.error.code)
                ui.refresh()
                return
            end
            if response.status ~= 200 then
                state = "HTTP " .. tostring(response.status)
                ui.refresh()
                return
            end
            local data = json.decode(response.body)
            if type(data) ~= "table" or type(data.hitokoto) ~= "string" then
                state = "返回格式不认识"
                ui.refresh()
                return
            end
            -- 出处跟在正文后面, 当作最后一行参与折行/滚动 —— 省一段文字额度
            local from = type(data.from) == "string" and #data.from > 0 and data.from or nil
            lines = wrap(from and (data.hitokoto .. "\n—— " .. from) or data.hitokoto, WRAP_PX)
            scroll = 0
            state = "已更新"
            ui.refresh()
        end,
        leave = function(self)
            if req_id and ui.HAS_HTTP then http.cancel(req_id); req_id = nil end
        end,
    })
end
