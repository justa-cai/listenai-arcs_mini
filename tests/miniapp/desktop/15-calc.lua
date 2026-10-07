-- 计算器：4×5 键盘，方向键移动（A = 按下当前键），无手柄时单击移动、双击按下。
-- 每帧只有 8 段文字，19 个键不可能都写字，所以键盘画成矩形网格、只显示"当前
-- 选中的键"。表达式内部用 ASCII 的 + - * /，显示时再映射成 × ÷。
do
    local C = ui.C
    local W, H = ui.W, ui.H

    local KEYS = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "0", ".", "=", "+",
        "C", "B", "ESC",         -- C 清空 / B 退格 / ESC 退出
    }
    local NKEY = #KEYS

    local X0, Y0, CW, CH, GAP = 4, 76, 56, 30, 2
    local expr, result, sel = "", "", 1

    local function key_rect(i)
        if i <= 16 then
            local c, r = (i - 1) % 4, math.floor((i - 1) / 4)
            return X0 + c * (CW + GAP), Y0 + r * (CH + GAP), CW, CH
        elseif i <= 18 then
            local c = i - 17
            return X0 + c * (CW + GAP), Y0 + 4 * (CH + GAP), CW, CH
        end
        return X0 + 2 * (CW + GAP), Y0 + 4 * (CH + GAP), CW * 2 + GAP, CH
    end

    local function display_op(s)
        s = string.gsub(s, "%*", "×")
        s = string.gsub(s, "/", "÷")
        return s
    end

    -- 没有 load/pcall，自己按"先乘除后加减"求值
    local function evaluate(s)
        local nums, ops, i = {}, {}, 1
        while i <= #s do
            local c = string.sub(s, i, i)
            if c == " " then
                i = i + 1
            elseif string.find("0123456789.", c, 1, true) then
                local j = i
                while j <= #s and string.find("0123456789.", string.sub(s, j, j), 1, true) do
                    j = j + 1
                end
                local v = tonumber(string.sub(s, i, j - 1))
                if not v then return nil, "数字格式错" end
                nums[#nums + 1] = v
                i = j
            elseif string.find("+-*/", c, 1, true) then
                if #nums ~= #ops + 1 then return nil, "运算符位置错" end
                ops[#ops + 1] = c
                i = i + 1
            else
                return nil, "非法字符"
            end
        end
        if #nums == 0 or #nums ~= #ops + 1 then return nil, "表达式不完整" end

        local n2, o2 = { nums[1] }, {}
        for k = 1, #ops do
            local op, v = ops[k], nums[k + 1]
            if op == "*" then
                n2[#n2] = n2[#n2] * v
            elseif op == "/" then
                if v == 0 then return nil, "除数为 0" end
                n2[#n2] = n2[#n2] / v
            else
                o2[#o2 + 1] = op
                n2[#n2 + 1] = v
            end
        end
        local acc = n2[1]
        for k = 1, #o2 do
            if o2[k] == "+" then acc = acc + n2[k + 1] else acc = acc - n2[k + 1] end
        end
        return acc, nil
    end

    local function fmt(v)
        if v == math.floor(v) and math.abs(v) < 1e12 then return string.format("%d", v) end
        return string.format("%.6g", v)
    end

    local function press()
        local k = KEYS[sel]
        if k == "ESC" then
            ui.back()
            return
        end
        -- 一按新键就把上一次的答案收起来: 显示行是"表达式=结果"拼在一起的，
        -- 不收起的话按完 = 再按 + 会显示成 "12+3+=15" 这种怪东西
        if k ~= "=" then result = "" end

        if k == "C" then
            expr = ""
        elseif k == "B" then
            expr = string.sub(expr, 1, math.max(0, #expr - 1))
        elseif k == "=" then
            if #expr > 0 then
                local v, err = evaluate(expr)
                if v then
                    result = fmt(v)
                    ui.beep(1200, 40)
                else
                    result = err
                    ui.beep(300, 60)
                end
            end
        else
            if string.find("+-*/", k, 1, true) then
                -- 两个运算符相邻时用新的替换
                local last = string.sub(expr, -1)
                if last ~= "" and string.find("+-*/", last, 1, true) then
                    expr = string.sub(expr, 1, #expr - 1)
                end
                if #expr == 0 and k ~= "-" then return end
            end
            if #expr < 24 then expr = expr .. k end
        end
        ui.refresh()
    end

    -- 表达式 + 结果拼成一行。每帧只有 8 段文字、键面 5 行已占 5 段，所以这里
    -- 只留 1 段(第 7 段留作壳层的提示行)。放不下时从左边截 —— 最新输入的尾巴
    -- 必须可见。整行 ≤29 个 ASCII 字符 (29*8 = 232px ≤ 240)。
    local function value_line()
        local e = display_op(expr)
        if result == "" then
            return e ~= "" and e or "0"
        end
        local s = e .. "=" .. result
        if #s <= 29 then return s end
        return string.sub(s, #s - 28)
    end

    -- 键面标签。每帧只有 8 段文字，19 个键逐个画会超额度，所以"一行"预拼成一条
    -- 字符串，把每个数字用空格顶到它那个键的中心。
    --
    -- 右下角那个跨两格的宽键是 ESC：显式的"离开计算器"。它和手柄的"返回"
    -- (圆) 是两条独立的路 —— 手柄返回不看光标在哪，ESC 得先把光标移过去才按得动。
    --
    -- 空格数不能按"每字符 8px"算：这台机器上不是等宽字体。实测
    -- res/arcs-mini/respak/font/lv_font_chinese_16.bin (advance 单位 1/16 px)：
    -- 空格 57(3.6px)、数字/+/= 141(8.8px)、. 69、- 88、* 118、/ 100、
    -- C 162、B 167、X 144。下面这 5 条就是按这些真实宽度解出来的(误差 ≤2px)，
    -- 改键位几何时必须重新解一遍。
    local ROWS = {
        "       7             8              9              /",
        "       4             5              6              *",
        "       1             2              3               -",
        "       0              .              =              +",
        "      C              B                   ESC",
    }

    local function draw()
        ui.begin("计算器")
        ui.text(value_line(), 10, 40, C.text)

        -- 顺序很重要: 先铺键面底色再写字，反过来标签会被底色盖掉。
        -- 键面用暗色(未选 tile_dim / 选中 tile)、标签用亮色，两边对比都够；
        -- 选中的键用亮边框圈出来 —— 之前是"填充内框"，那会盖掉键面文字。
        for i = 1, NKEY do
            local x, y, w, h = key_rect(i)
            local on = (i == sel)
            screen.rect(x, y, w, h, on and C.tile or C.tile_dim)
            if on then
                screen.rect(x, y, w, 2, C.hi)
                screen.rect(x, y + h - 2, w, 2, C.hi)
                screen.rect(x, y, 2, h, C.hi)
                screen.rect(x + w - 2, y, 2, h, C.hi)
            end
        end

        -- 标签: 一行一条字符串，共 5 条 + 上面 1 条 = 6/8 段。
        -- y 就取键的顶边: 字体行高 30 = 键高 30，且字墨迹在行框里垂直居中
        -- (基线在行顶下 21、数字高 12)，所以 y = 键顶 就是垂直居中。
        for r = 1, 5 do
            screen.text(ROWS[r], X0, Y0 + (r - 1) * (CH + GAP), C.text)
        end
    end

    ui.register({
        id = "calc",
        name = "计算器",
        icon = "calc",
        color = C.warn,
        raw = true,                    -- 自己收方向键（移光标）与 A/双击（按键）
        enter = function(self) sel = 1; ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        click = function(self, id)
            -- 4 列网格 + 最后一行是 C/B/X：上下键跨 4 格，左右键跨 1 格
            if id == "right" then
                sel = sel % NKEY + 1
            elseif id == "left" then
                sel = (sel - 2) % NKEY + 1
            elseif id == "down" then
                sel = (sel - 1 + 4) % NKEY + 1
            elseif id == "up" then
                sel = (sel - 1 - 4) % NKEY + 1
            elseif id == "function_double" then
                press()
            else
                sel = sel % NKEY + 1        -- function: 设备功能键单击 = 下一个
            end
            ui.refresh()
        end,
        leave = function(self) end,
    })
end
