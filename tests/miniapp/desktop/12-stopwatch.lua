-- 秒表：按 tick 计数计时（dt 恒 20ms），不依赖系统时钟，未校时也能用。
do
    local C = ui.C
    local elapsed, running, last_cs = 0, false, -1

    local function label()
        local cs = math.floor(elapsed / 100) % 10
        local total_s = math.floor(elapsed / 1000)
        return string.format("%02d:%02d.%d", math.floor(total_s / 60) % 100, total_s % 60, cs)
    end

    local function toggle()
        running = not running
        ui.beep(running and 1200 or 800, 40)
        ui.refresh()
    end

    local function reset()
        elapsed, running, last_cs = 0, false, -1
        ui.refresh()
    end

    local function draw()
        ui.begin("秒表")
        ui.text_center(label(), 92, C.hi)
        ui.text_center(running and "计时中" or "已暂停", 132, C.dim)
        ui.commands({
            { label = "返回", run = ui.back },
            { label = running and "暂停" or "开始", run = toggle },
            { label = "归零", run = reset },
        })
    end

    ui.register({
        id = "stopwatch",
        name = "秒表",
        icon = "stopwatch",
        color = C.hi,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt)
            if not running then return end
            elapsed = elapsed + dt
            local cs = math.floor(elapsed / 100) % 10
            if cs ~= last_cs then
                last_cs = cs
                ui.refresh()
            end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
