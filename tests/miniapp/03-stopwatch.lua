-- API 3 positive case: wall-clock seconds, single/double click and sound.
-- Tick only refreshes the display; elapsed time includes alarm suspension.
assert(app.api_version >= 3, "STOPWATCH: API 3 required")
assert(screen and app.width >= 200 and app.height >= 200, "STOPWATCH: screen >= 200x200 required")
local W, H = app.width, app.height
local input_ms, accumulated, started_at = 0, 0, 0
local last_display = nil
local running, last_click = false, nil
local last_click_second = nil
local DOUBLE_MS = 300
local segments = {"abcdef", "bc", "abdeg", "abcdg", "bcfg",
                  "acdfg", "acdefg", "abc", "abcdefg", "abcdfg"}
-- Large MM:SS display with separate status/footer zones.
local function draw_time(value)
    local count = #value - 1 -- Exclude the colon.
    local width = math.floor(math.min(40, (W - 24) / count, (H - 152) / 2))
    local thickness, gap, total
    repeat
        thickness, gap = math.max(2, math.floor(width / 5)), math.max(2, math.floor(width / 8))
        total = count * width + (#value - 1) * gap + 2 * thickness
        if total <= W - 24 then break end
        width = width - 1
    until width <= 4
    local height = width * 2
    local middle = math.floor((height - thickness) / 2)
    local shapes = {
        a = {thickness, 0, width - 2 * thickness, thickness},
        b = {width - thickness, thickness, thickness, middle - thickness},
        c = {width - thickness, middle + thickness, thickness, height - middle - 2 * thickness},
        d = {thickness, height - thickness, width - 2 * thickness, thickness},
        e = {0, middle + thickness, thickness, height - middle - 2 * thickness},
        f = {0, thickness, thickness, middle - thickness},
        g = {thickness, middle, width - 2 * thickness, thickness}
    }
    local x = math.floor((W - total) / 2)
    local y = 68 + math.floor((H - 144 - height) / 2)
    for char in value:gmatch(".") do
        if char == ":" then
            local dot_x = x + math.floor(thickness / 2)
            screen.rect(dot_x, y + math.floor(height / 4), thickness, thickness, 0x71E4AD)
            screen.rect(dot_x, y + math.floor(3 * height / 4), thickness, thickness, 0x71E4AD)
            x = x + 2 * thickness + gap
        else
            for name in segments[tonumber(char) + 1]:gmatch(".") do
                local r = shapes[name]
                screen.rect(x + r[1], y + r[2], r[3], r[4], 0xFFFFFF)
            end
            x = x + width + gap
        end
    end
end

local function elapsed(at)
    return accumulated + (running and math.max(0, at - started_at) or 0)
end

local function tone(hz)
    if buzzer then buzzer.play(hz, 200) end
end

local function draw()
    local at = clock.now()
    local status = not at and "等待设备校时" or (running and "运行中" or "已暂停")
    local seconds = at and elapsed(at) or nil
    local display = status .. ":" .. tostring(seconds)
    if display == last_display then return end
    last_display = display
    screen.begin(0x101923)
    screen.text("秒表", 12, 12, 0xFFFFFF)
    screen.text(status, 12, 44, 0x71E4AD)
    if seconds then
        draw_time(string.format("%02d:%02d", math.floor(seconds / 60), seconds % 60))
    end
    screen.text("单击开始 / 暂停", 12, H - 64, 0xFFFFFF)
    screen.text("双击清零并暂停", 12, H - 44, 0xFFFFFF)
    screen.text(buzzer and "操作时蜂鸣" or "声音：未提供接口", 12, H - 24, 0xFFB454)
    screen.present()
end

local function single(at)
    if running then
        accumulated = elapsed(at)
        running = false
        tone(660)
    else
        started_at, running = at, true
        tone(880)
    end
end

function on_start() draw() end

function on_tick(dt_ms)
    assert(type(dt_ms) == "number" and dt_ms >= 0, "STOPWATCH: invalid tick")
    -- This approximate tick counter is only used for click grouping, never timing.
    input_ms = input_ms + dt_ms
    if last_click and input_ms - last_click > DOUBLE_MS then last_click = nil end
    -- Poll every tick and present as soon as the wall-clock second changes.
    draw()
end

function on_button_click(button_id)
    if button_id ~= "function" then return end
    local at = clock.now()
    if not at then last_click = nil; draw(); return end
    -- Do not pair clicks across a long interval with no ticks (e.g. an alarm).
    if last_click and input_ms - last_click <= DOUBLE_MS
       and at >= last_click_second and at - last_click_second <= 1 then
        last_click, running, accumulated = nil, false, 0
        tone(440)
    else
        -- Respond to the first click immediately; a second click resets the timer.
        last_click, last_click_second = input_ms, at
        single(at)
    end
    draw()
end
