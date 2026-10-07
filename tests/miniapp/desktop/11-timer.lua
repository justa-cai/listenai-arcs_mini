-- 倒计时：只在应用打开期间有效（设备没有给小应用的闹钟 API），到点用蜂鸣提醒。
-- 计时用 tick 计数，不依赖系统时钟。
do
    local C = ui.C
    local minutes, remain_ms, running = 1, 60 * 1000, false
    local fired_note, beep_left = "", 0

    local function fmt(ms)
        if ms < 0 then ms = 0 end
        local s = math.ceil(ms / 1000)
        return string.format("%02d:%02d", math.floor(s / 60), s % 60)
    end

    local function draw()
        ui.begin("倒计时")
        ui.text_center(fmt(remain_ms), 86, remain_ms <= 0 and C.bad or C.hi)
        ui.text_center(running and "计时中" or (remain_ms <= 0 and "时间到" or "已暂停"), 128, C.dim)
        if fired_note ~= "" then ui.footer(fired_note, C.warn) end

        ui.commands({
            { label = "返回", run = ui.back },
            { label = running and "暂停" or "开始", run = function()
                  if remain_ms <= 0 then remain_ms = minutes * 60000 end
                  running = not running
                  fired_note = ""
                  ui.refresh()
              end },
            { label = "加 1 分钟", run = function()
                  minutes = minutes % 60 + 1
                  remain_ms = minutes * 60000
                  running = false
                  fired_note = ""
                  ui.refresh()
              end },
            { label = "归零", run = function()
                  running = false
                  remain_ms = minutes * 60000
                  fired_note = ""
                  ui.refresh()
              end },
        })
    end

    ui.register({
        id = "timer",
        name = "倒计时",
        icon = "timer",
        color = C.warn,
        enter = function(self)
            remain_ms = minutes * 60000
            running = false
            fired_note = ""
            beep_left = 0
            ui.refresh()
        end,
        tick = function(self, dt)
            if beep_left > 0 then
                -- 每 4 个 tick（约 80ms）响一次，共 4 次
                if beep_left % 4 == 0 then ui.beep(1500, 60) end
                beep_left = beep_left - 1
            end
            if not running then return end
            remain_ms = remain_ms - dt
            if remain_ms <= 0 then
                running = false
                remain_ms = 0
                fired_note = "时间到"
                beep_left = 16
                ui.led("fast")
                ui.refresh()
            else
                ui.refresh()                       -- 秒在跳，每 tick 重画
            end
        end,
        draw = function(self) draw() end,
        leave = function(self)
            running = false
            ui.led("off")
        end,
    })
end
