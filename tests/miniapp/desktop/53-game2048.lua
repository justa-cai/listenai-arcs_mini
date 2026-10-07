-- 2048：每帧只有 8 段文字，16 个格子写不下 16 个数字，所以按"4 行文本"渲染。
-- 方向键直接走棋（无手柄时用命令条里的四个方向）；棋盘满了才把命令条让回来。
do
    local C = ui.C
    local b = {}
    local score, best, moves, state

    local function empty_cells()
        local out = {}
        for r = 1, 4 do
            for c = 1, 4 do
                if b[r][c] == 0 then out[#out + 1] = {r = r, c = c} end
            end
        end
        return out
    end

    local function add_tile()
        local e = empty_cells()
        if #e == 0 then return end
        local pick = e[math.random(1, #e)]
        b[pick.r][pick.c] = (math.random(1, 10) == 1) and 4 or 2
    end

    local function reset()
        b = {}
        for r = 1, 4 do b[r] = {0, 0, 0, 0} end
        score, moves, state = 0, 0, "playing"
        add_tile()
        add_tile()
    end

    -- 把一行/一列往"前"压合并；返回 {changed, values}
    local function slide(line)
        local kept = {}
        for i = 1, 4 do
            if line[i] ~= 0 then kept[#kept + 1] = line[i] end
        end
        local out, gained, i = {}, 0, 1
        while i <= #kept do
            if i < #kept and kept[i] == kept[i + 1] then
                local v = kept[i] * 2
                out[#out + 1] = v
                gained = gained + v
                i = i + 2
            else
                out[#out + 1] = kept[i]
                i = i + 1
            end
        end
        while #out < 4 do out[#out + 1] = 0 end
        return out, gained
    end

    local function read_line(dir, idx)
        local line = {}
        for k = 1, 4 do
            if dir == "left" then line[k] = b[idx][k]
            elseif dir == "right" then line[k] = b[idx][5 - k]
            elseif dir == "up" then line[k] = b[k][idx]
            else line[k] = b[5 - k][idx] end
        end
        return line
    end

    local function write_line(dir, idx, line)
        for k = 1, 4 do
            local v = line[k]
            if dir == "left" then b[idx][k] = v
            elseif dir == "right" then b[idx][5 - k] = v
            elseif dir == "up" then b[k][idx] = v
            else b[5 - k][idx] = v end
        end
    end

    local function move(dir)
        if state ~= "playing" then return end
        local changed = false
        for idx = 1, 4 do
            local before = read_line(dir, idx)
            local line, gained = slide(before)
            write_line(dir, idx, line)
            if gained > 0 then
                score = score + gained
                changed = true
            end
            for k = 1, 4 do
                if before[k] ~= line[k] then changed = true end
            end
        end
        if changed then
            moves = moves + 1
            add_tile()
            if score > best then
                best = score
                ui.save("g2048.best", best)
            end
            ui.beep(900, 25)
            if #empty_cells() == 0 then state = "full" end
        else
            ui.beep(300, 40)
        end
        ui.refresh()
    end

    local function row_text(r)
        local parts = {}
        for c = 1, 4 do
            local v = b[r][c]
            parts[c] = string.format("%4s", v == 0 and "." or tostring(v))
        end
        return table.concat(parts, " ")
    end

    local function draw()
        ui.begin("2048")
        ui.text(string.format("得分 %d   最高 %d   步 %d%s", score, best, moves,
                              state == "full" and "  · 已满" or ""), 12, 38, C.text)
        for r = 1, 4 do
            ui.text(row_text(r), 16, 72 + (r - 1) * 26, C.hi)
        end
        if state == "playing" then
            -- 游玩中让出命令条: 方向键直接走棋, 免得每一步都先移光标再按确认
            ui.commands_clear()
        else
            ui.commands({
                { label = "返回", run = ui.back },
                { label = "重开", run = function()
                      math.randomseed(ui.ms() + 11); reset(); ui.refresh() end },
            })
        end
    end

    ui.register({
        id = "g2048",
        name = "2048",
        icon = "g2048",
        color = C.hi,
        raw = true,
        enter = function(self)
            math.randomseed(ui.ms() + 11)
            best = ui.load("g2048.best", 0)
            if type(best) ~= "number" then best = 0 end
            reset()
            ui.refresh()
        end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        click = function(self, id)
            if id == "left" then move("left")
            elseif id == "right" then move("right")
            elseif id == "up" then move("up")
            elseif id == "down" then move("down") end
        end,
        leave = function(self) end,
    })
end
