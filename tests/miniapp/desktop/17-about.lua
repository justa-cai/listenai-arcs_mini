-- 关于：运行环境信息（API 版本、画布、能力开关、存档用量、当前时间）
do
    local C = ui.C
    local W, H = ui.W, ui.H
    local last_key = ""

    local function capability_line()
        local caps = {}
        caps[#caps + 1] = ui.HAS_LED and "灯" or "无灯"
        caps[#caps + 1] = ui.HAS_BUZZER and "蜂鸣" or "无蜂鸣"
        caps[#caps + 1] = ui.HAS_HTTP and "网络" or "无网络"
        caps[#caps + 1] = ui.HAS_TTS and "播报" or "无播报"
        return table.concat(caps, " ")
    end

    local function draw()
        ui.begin("关于")
        local t = ui.localtime()
        ui.text("LingClaw Lua 桌面", 12, 40, C.text)
        ui.text(string.format("API v%d   画布 %dx%d", app.api_version, W, H), 12, 64, C.dim)
        ui.text("能力: " .. capability_line(), 12, 88, C.dim)
        ui.text(string.format("存档: %d 项", ui.save_key_count()), 12, 112, C.dim)
        if t then
            ui.text(string.format("时间: %04d-%02d-%02d %02d:%02d",
                                  t.year, t.month, t.day, t.hour, t.min), 12, 136, C.dim)
        else
            ui.text("时间: 未校时", 12, 136, C.warn)
        end
        local note = ui.save_note()
        if note ~= "" then ui.footer(note, C.warn) end
        ui.commands({ {label = "返回", run = ui.back} })
    end

    ui.register({
        id = "about",
        name = "关于",
        icon = "about",
        color = C.dim,
        enter = function(self) last_key = ""; ui.refresh() end,
        tick = function(self, dt)
            -- 存档项数/时间会变，变化时才重画
            local t = ui.localtime()
            local k = tostring(ui.save_key_count()) .. ":" .. tostring(t and t.min)
            if k ~= last_key then last_key = k; ui.refresh() end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
