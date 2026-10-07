-- 设置：桌面手势、蜂鸣开关、清空存档。改动的开关立即写进桌面共用存档。
do
    local C = ui.C

    local function draw()
        ui.begin("设置")
        ui.text("手势: " .. (ui.dwell_on() and "停留打开" or "双击打开"), 12, 44, C.dim)
        ui.text("蜂鸣: " .. (ui.beep_on() and "开" or "关"), 12, 68, C.dim)
        ui.text(string.format("存档: %d 项", ui.save_key_count()), 12, 92, C.dim)
        local note = ui.save_note()
        if note ~= "" then ui.footer(note, C.warn) end

        ui.commands({
            { label = "返回", run = ui.back },
            { label = ui.dwell_on() and "改为双击打开" or "改为停留打开",
              run = function()
                  ui.set_dwell(not ui.dwell_on())
                  ui.notice(ui.dwell_on() and "停留打开" or "双击打开")
              end },
            { label = ui.beep_on() and "关闭蜂鸣" or "打开蜂鸣",
              run = function()
                  local on = not ui.beep_on()
                  ui.set_beep(on)
                  if on then ui.beep(1000, 50) end
                  ui.notice(on and "蜂鸣已开" or "蜂鸣已关")
              end },
            { label = "试听蜂鸣", run = function() ui.beep(880, 120) end },
            { label = "清空存档", run = function()
                  ui.save_clear()
                  ui.notice("存档将在下次写入时清空")
              end },
        })
    end

    ui.register({
        id = "settings",
        name = "设置",
        icon = "settings",
        color = C.dim,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
