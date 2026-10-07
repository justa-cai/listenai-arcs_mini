-- 状态灯：切换熄灭 / 常亮 / 慢闪 / 快闪，离开时关灯。
do
    local C = ui.C
    local MODES = { {label = "熄灭", led = "off"}, {label = "常亮", led = "on"},
                    {label = "慢闪", led = "slow"}, {label = "快闪", led = "fast"} }
    local current = 1

    local function pick(i)
        current = i
        if ui.HAS_LED then ui.led(MODES[i].led) end
        ui.refresh()
    end

    local function draw()
        ui.begin("状态灯")
        if not ui.HAS_LED then
            ui.text_center("本机无 LED 接口", 96, C.dim)
        else
            ui.text_center(MODES[current].label, 92, C.hi)
            screen.rect(85, 128, 70, 40, current == 1 and C.tile_dim or C.hi)
            ui.text_center("请观察实体状态灯", 176, C.dim)
        end

        local items = { { label = "返回", run = function() pick(1); ui.back() end } }
        for i = 1, #MODES do
            items[#items + 1] = { label = MODES[i].label, run = function() pick(i) end }
        end
        ui.commands(items)
    end

    ui.register({
        id = "led",
        name = "状态灯",
        icon = "led",
        color = C.hi,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
