-- API 2 positive case: off -> on -> slow blink -> fast blink -> off.
assert(app.api_version >= 2, "LED: API 2 required")
assert(screen and app.width >= 200 and app.height >= 200, "LED: screen >= 200x200 required")
local W, H = app.width, app.height
local mode, clicks = 1, 0
local labels = {"熄灭", "常亮", "慢闪", "快闪"}

local function apply()
    if not led then return end
    if mode == 1 then led.off("status")
    elseif mode == 2 then led.on("status")
    elseif mode == 3 then led.blink("status", 500, 500)
    else led.blink("status", 100, 900) end
end

local function draw()
    screen.begin(0x101923)
    screen.text("状态灯测试", 12, 12, 0xFFFFFF)
    screen.text(led and labels[mode] or "SKIP：无 LED 接口", 12, 48, 0x71E4AD)
    -- This is a requested-mode indicator, not an emulation of the physical LED.
    screen.rect(math.floor(W / 2) - 20, 80, 40, 32,
                led and mode ~= 1 and 0x71E4AD or 0x425368)
    local timing = {"亮 0 ms", "持续点亮", "亮 500 / 灭 500 ms", "亮 100 / 灭 900 ms"}
    screen.text(timing[mode], 12, H - 88, 0xA5B5C5)
    screen.text("按键次数 " .. clicks, 12, H - 64, 0xA5B5C5)
    screen.text("单击切换灯光模式", 12, H - 44, 0xFFFFFF)
    screen.text("请观察实体状态灯", 12, H - 24, 0xFFB454)
    screen.present()
end

function on_start() apply(); draw() end
function on_tick(dt_ms) end
function on_button_click(button_id)
    if button_id ~= "function" then return end
    clicks, mode = clicks + 1, mode % 4 + 1
    apply()
    draw()
end
function on_exit()
    if led then led.off("status") end
end
