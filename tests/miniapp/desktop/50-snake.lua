-- 贪吃蛇：方向键直接转向；无手柄时单击顺时针一格、双击转两格。棋盘 10×8。
do
    local C, W, H = ui.C, ui.W, ui.H
    local COLS, ROWS = 10, 8
    local TOP = 30
    -- 给底部命令条留出 44px，避免棋盘压在命令条下面
    local cell = math.floor(math.min((W - 8) / COLS, (H - TOP - 44) / ROWS))
    local gx = math.floor((W - COLS * cell) / 2)
    local gy = TOP

    local DX, DY = {1, 0, -1, 0}, {0, 1, 0, -1}
    local snake, food, dir, score, best, state, acc
    local STEP = 320

    local function place_food()
        for offset = 0, COLS * ROWS - 1 do
            local n = (score * 17 + offset) % (COLS * ROWS)
            local x, y = n % COLS, math.floor(n / COLS)
            local taken = false
            for _, p in ipairs(snake) do
                if p.x == x and p.y == y then taken = true; break end
            end
            if not taken then food = {x = x, y = y}; return end
        end
        food = nil
        state = "won"
    end

    local function reset()
        snake = { {x = 5, y = 5}, {x = 4, y = 5}, {x = 3, y = 5} }
        dir, score, acc, state = 1, 0, 0, "ready"
        food = {x = 8, y = 5}
    end

    local function step()
        local x, y = snake[1].x + DX[dir], snake[1].y + DY[dir]
        if x < 0 or x >= COLS or y < 0 or y >= ROWS then state = "over"; return end
        local eating = food and x == food.x and y == food.y
        for i = 1, #snake - (eating and 0 or 1) do
            if snake[i].x == x and snake[i].y == y then state = "over"; return end
        end
        table.insert(snake, 1, {x = x, y = y})
        if eating then
            score = score + 1
            if score > best then
                best = score
                ui.save("snake.best", best)
            end
            ui.beep(1400, 30)
            place_food()
        else
            table.remove(snake)
        end
    end

    local function menu()
        if state == "playing" then
            -- 游玩中不要命令条，否则按键会被命令条吃掉
            ui.commands_clear()
            return
        end
        ui.commands({
            { label = "返回", run = ui.back },
            { label = (state == "ready") and "开始" or "重开",
              run = function() reset(); state = "playing"; ui.refresh() end },
        })
    end

    local function draw()
        ui.begin("贪吃蛇")
        ui.text(string.format("得分 %d   最高 %d %s", score, best,
                              state == "over" and "· 结束" or ""), 12, 32, C.text)

        screen.rect(gx - 2, gy - 2, COLS * cell + 4, ROWS * cell + 4, C.line)
        screen.rect(gx, gy, COLS * cell, ROWS * cell, 0x12202A)
        if food then
            screen.rect(gx + food.x * cell + 1, gy + food.y * cell + 1,
                        cell - 2, cell - 2, C.warn)
        end
        for i, p in ipairs(snake) do
            screen.rect(gx + p.x * cell + 1, gy + p.y * cell + 1,
                        cell - 2, cell - 2, i == 1 and C.text or C.hi)
        end
        menu()
    end

    ui.register({
        id = "snake",
        name = "贪吃蛇",
        icon = "snake",
        color = C.hi,
        raw = true,
        enter = function(self)
            best = ui.load("snake.best", 0)
            if type(best) ~= "number" then best = 0 end
            reset()
            ui.refresh()
        end,
        tick = function(self, dt)
            if state ~= "playing" then return end
            acc = acc + dt
            while acc >= STEP and state == "playing" do
                acc = acc - STEP
                step()
            end
            ui.refresh()
        end,
        draw = function(self) draw() end,
        click = function(self, id)
            if state ~= "playing" then return end
            -- DX/DY 顺序: 1=右 2=下 3=左 4=上。
            -- 手柄 A (function_double) 也走"顺时针一格"这条: 它是独立按键、
            -- 按下就是一次动作，不该理解成"按两下"。
            local want
            if id == "right" then want = 1
            elseif id == "down" then want = 2
            elseif id == "left" then want = 3
            elseif id == "up" then want = 4
            else want = dir % 4 + 1 end
            -- 不能原地 180° 掉头 (那等于撞自己的脖子)
            if want ~= (dir + 1) % 4 + 1 then dir = want end
            ui.refresh()
        end,
        leave = function(self) end,
    })
end
