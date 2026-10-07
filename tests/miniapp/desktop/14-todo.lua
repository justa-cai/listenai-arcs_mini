-- 待办清单：预置条目 + 单键勾选（没有键盘，不支持自由输入）。
-- 勾选状态以 "0101" 形式的字符串存进桌面共用的存档。
do
    local C = ui.C
    local ITEMS = { "喝水", "起来走走", "回消息", "早点睡" }
    local index = 1
    local done = ""      -- 存档要在 enter 里读：壳层 on_start 之前 store 还是空的

    local function is_done(i)
        return string.sub(done, i, i) == "1"
    end

    local function toggle(i)
        local chars = {}
        for j = 1, #ITEMS do
            chars[j] = (j == i) and (is_done(j) and "0" or "1") or (is_done(j) and "1" or "0")
        end
        done = table.concat(chars)
        ui.save("todo.done", done)
        ui.beep(is_done(i) and 1200 or 700, 40)
        ui.refresh()
    end

    local function draw()
        ui.begin("待办")
        local y = 40
        for i = 1, #ITEMS do
            local on = (i == index)
            if on then screen.rect(6, y - 4, ui.W - 12, 26, C.tile) end
            local mark = is_done(i) and "[x] " or "[ ] "
            ui.text(mark .. ITEMS[i], 12, y, on and C.hi or C.text)
            y = y + 28
        end
        ui.commands({
            { label = "返回", run = ui.back },
            { label = "下一条", run = function()
                index = index % #ITEMS + 1
                ui.refresh()
            end },
            { label = is_done(index) and "取消勾选" or "勾选", run = function() toggle(index) end },
        })
    end

    ui.register({
        id = "todo",
        name = "待办",
        icon = "todo",
        color = C.hi,
        enter = function(self)
            index = 1
            local saved = ui.load("todo.done", "")
            if type(saved) == "string" then done = saved end
            ui.refresh()
        end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
