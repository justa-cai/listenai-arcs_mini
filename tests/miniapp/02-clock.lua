-- API 3 positive case: actual wall clock, never inferred from tick counts.
assert(app.api_version >= 3, "CLOCK: API 3 required")
assert(screen and app.width >= 200 and app.height >= 200, "CLOCK: screen >= 200x200 required")
local W, H = app.width, app.height
local refresh, previous = 0, nil
local segments = {"abcdef", "bc", "abdeg", "abcdg", "bcfg",
                  "acdfg", "acdefg", "abc", "abcdefg", "abcdfg"}

local function digit(value, x, y, scale)
    local shapes = {
        a = {1, 0, 3, 1}, b = {4, 1, 1, 3}, c = {4, 5, 1, 3},
        d = {1, 8, 3, 1}, e = {0, 5, 1, 3}, f = {0, 1, 1, 3}, g = {1, 4, 3, 1}
    }
    for name in segments[value + 1]:gmatch(".") do
        local r = shapes[name]
        screen.rect(x + r[1] * scale, y + r[2] * scale,
                    r[3] * scale, r[4] * scale, 0x71E4AD)
    end
end

local function draw()
    local unix, t = clock.now(), clock.localtime()
    local key = t and string.format("%s:%d:%d", tostring(unix), t.sec, t.utc_offset) or "waiting"
    if key == previous then return end
    previous = key
    screen.begin(0x101923)
    screen.text("真实时间", 12, 12, 0xFFFFFF)
    if not unix or not t then
        screen.text("等待设备校时", 12, 72, 0xFFB454)
        screen.text("校时后自动显示", 12, 104, 0xA5B5C5)
    else
        assert(type(unix) == "number" and unix % 1 == 0, "CLOCK: invalid Unix seconds")
        assert(t.month >= 1 and t.month <= 12 and t.day >= 1 and t.day <= 31,
               "CLOCK: invalid date")
        assert(t.hour >= 0 and t.hour <= 23 and t.min >= 0 and t.min <= 59
               and t.sec >= 0 and t.sec <= 60 and t.wday >= 0 and t.wday <= 6,
               "CLOCK: invalid local time")
        local scale = math.max(1, math.floor((W - 24) / 40))
        local x, y = math.floor((W - 40 * scale) / 2), 60
        local digits = string.format("%02d%02d%02d", t.hour, t.min, t.sec)
        for i = 1, 6 do
            digit(tonumber(digits:sub(i, i)), x, y, scale)
            x = x + 6 * scale
            if i == 2 or i == 4 then
                screen.rect(x, y + 2 * scale, scale, scale, 0xFFFFFF)
                screen.rect(x, y + 6 * scale, scale, scale, 0xFFFFFF)
                x = x + 2 * scale
            end
        end
        screen.text(string.format("%04d-%02d-%02d", t.year, t.month, t.day), 12, H - 76, 0xFFFFFF)
        screen.text("Unix " .. tostring(unix), 12, H - 52, 0xA5B5C5)
        local offset = math.abs(t.utc_offset)
        screen.text(string.format("UTC%s%02d:%02d", t.utc_offset < 0 and "-" or "+",
                    math.floor(offset / 3600), math.floor(offset / 60) % 60), 12, H - 28, 0xA5B5C5)
    end
    screen.present()
end

function on_start() draw() end
function on_tick(dt_ms)
    refresh = refresh + dt_ms
    if refresh >= 100 then refresh = refresh % 100; draw() end
end
