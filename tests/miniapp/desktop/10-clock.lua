-- 时钟：真实日历时间（七段码画法取自 tests/miniapp/02-clock.lua）
-- 未校时时显示等待，不把 tick 累加值当作时间。
-- 结构约定：tick 只判断"要不要重画"，绘制统一放在 draw 里，壳层负责 present
-- 和底部的命令条（所以内容不要画到 y > H-40 的区域）。
do
    local C = ui.C
    local W, H = ui.W, ui.H

    local SEG = { "abcdef", "bc", "abdeg", "abcdg", "bcfg",
                  "acdfg", "acdefg", "abc", "abcdefg", "abcdfg" }
    local SHAPE = { a = {1, 0, 3, 1}, b = {4, 1, 1, 3}, c = {4, 5, 1, 3},
                    d = {1, 8, 3, 1}, e = {0, 5, 1, 3}, f = {0, 1, 1, 3}, g = {1, 4, 3, 1} }

    local function digit(value, x, y, s)
        for name in SEG[value + 1]:gmatch(".") do
            local r = SHAPE[name]
            screen.rect(x + r[1] * s, y + r[2] * s, r[3] * s, r[4] * s, C.hi)
        end
    end

    local last_key = ""

    local function current_key()
        local t = ui.localtime()
        if not t then return "wait" end
        return string.format("%d-%d-%d %d:%d:%d", t.year, t.month, t.day, t.hour, t.min, t.sec)
    end

    local function draw()
        local t = ui.localtime()
        ui.begin("时钟")

        if not t then
            ui.text_center("等待设备校时", 108, C.warn)
        else
            local s = math.max(1, math.floor((W - 32) / 40))
            local x = math.floor((W - 40 * s) / 2)
            local y = 54
            local ds = string.format("%02d%02d%02d", t.hour, t.min, t.sec)
            for i = 1, 6 do
                digit(tonumber(string.sub(ds, i, i)), x, y, s)
                x = x + 6 * s
                if i == 2 or i == 4 then
                    screen.rect(x, y + 2 * s, s, s, C.text)
                    screen.rect(x, y + 6 * s, s, s, C.text)
                    x = x + 2 * s
                end
            end

            ui.text_center(string.format("%04d-%02d-%02d", t.year, t.month, t.day), 140, C.text)
            local off = t.utc_offset or 0
            ui.text_center(string.format("UTC%s%02d:%02d", off < 0 and "-" or "+",
                                         math.floor(math.abs(off) / 3600),
                                         math.floor(math.abs(off) / 60) % 60), 166, C.dim)
        end

        ui.commands({ {label = "返回", run = ui.back} })
    end

    ui.register({
        id = "clock",
        name = "时钟",
        icon = "clock",
        color = C.hi,
        enter = function(self)
            last_key = ""
            ui.refresh()
        end,
        tick = function(self, dt)
            local k = current_key()
            if k ~= last_key then
                last_key = k
                ui.refresh()
            end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
