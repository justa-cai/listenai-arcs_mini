-- API 3 positive case: drawing, clicks, tick-driven motion and high-score storage.
assert(app.api_version >= 3, "SNAKE: API 3 required")
assert(screen and app.width >= 200 and app.height >= 200, "SNAKE: screen >= 200x200 required")

local W, H = app.width, app.height
local COLS, ROWS, STEP = 12, 10, 400
local cell = math.floor(math.min((W - 16) / COLS, (H - 112) / ROWS))
local left, top = math.floor((W - COLS * cell) / 2), 64
local snake, food, direction, score, elapsed
local state, best, persisted = "ready", 0, 0
local save_status = "无存档"
local dx, dy = {1, 0, -1, 0}, {0, 1, 0, -1}

local function reset()
    snake = {{x = 5, y = 5}, {x = 4, y = 5}, {x = 3, y = 5}}
    food, direction, score, elapsed = {x = 8, y = 5}, 1, 0, 0
end

local function draw()
    screen.begin(0x101923)
    screen.text("贪吃蛇", 12, 8, 0xFFFFFF)
    screen.text(string.format("得分 %d  最高 %d", score, best), 12, 32, 0x71E4AD)
    screen.rect(left - 2, top - 2, COLS * cell + 4, ROWS * cell + 4, 0x425368)
    screen.rect(left, top, COLS * cell, ROWS * cell, 0x182B35)
    if food then
        screen.rect(left + food.x * cell + 1, top + food.y * cell + 1,
                    cell - 2, cell - 2, 0xFFB454)
    end
    for i, p in ipairs(snake) do
        screen.rect(left + p.x * cell + 1, top + p.y * cell + 1,
                    cell - 2, cell - 2, i == 1 and 0xFFFFFF or 0x71E4AD)
    end
    local labels = {ready = "单击开始", playing = "单击顺时针转向",
                    over = "游戏结束 · 单击重来", won = "满格成功 · 单击重来"}
    screen.text(labels[state], 12, H - 40, 0xFFFFFF)
    screen.text(save_status, 12, H - 20, 0xA5B5C5)
    screen.present()
end

local function save_best()
    if best <= persisted then return end
    local ok, reason = storage.save({best = best})
    if not ok then
        -- Keep playing; retry on the next game end, never write every tick.
        local labels = {clock_unavailable = "未校时，成绩未存",
                        rate_limited = "保存限流，下局重试", storage_full = "存档已满，成绩未存"}
        save_status = labels[reason] or "保存失败，成绩未存"
        return
    end
    local saved = storage.load()
    assert(saved and saved.best == best, "SNAKE: saved score did not round-trip")
    persisted, save_status = best, "最高分已保存"
end

local function finish(next_state)
    state = next_state
    save_best()
end

local function place_food()
    -- Deterministic placement makes a repeated test reproducible.
    for offset = 0, COLS * ROWS - 1 do
        local n = (score * 17 + offset) % (COLS * ROWS)
        local x, y, occupied = n % COLS, math.floor(n / COLS), false
        for _, p in ipairs(snake) do
            if p.x == x and p.y == y then occupied = true; break end
        end
        if not occupied then food = {x = x, y = y}; return end
    end
    food = nil
    finish("won")
end

local function step()
    local x, y = snake[1].x + dx[direction], snake[1].y + dy[direction]
    if x < 0 or x >= COLS or y < 0 or y >= ROWS then finish("over"); return end
    local eating = food and x == food.x and y == food.y
    -- The tail vacates its cell on a move that does not eat.
    for i = 1, #snake - (eating and 0 or 1) do
        if snake[i].x == x and snake[i].y == y then finish("over"); return end
    end
    table.insert(snake, 1, {x = x, y = y})
    if eating then
        score = score + 1
        best = math.max(best, score)
        place_food()
    else
        table.remove(snake)
    end
end

function on_start()
    local saved = storage.load()
    if saved and type(saved.best) == "number" and saved.best >= 0
       and saved.best <= COLS * ROWS - 3 and saved.best % 1 == 0 then
        best, persisted, save_status = saved.best, saved.best, "已恢复最高分"
    elseif clock.now() == nil then
        save_status = "未校时，存档待验证"
    end
    reset()
    draw()
end

function on_tick(dt_ms)
    assert(type(dt_ms) == "number" and dt_ms >= 0, "SNAKE: invalid tick")
    if state ~= "playing" then return end
    elapsed = elapsed + dt_ms
    local changed = false
    while elapsed >= STEP and state == "playing" do
        elapsed = elapsed - STEP
        step()
        changed = true
    end
    if changed then draw() end
end

function on_button_click(button_id)
    if button_id ~= "function" then return end
    if state == "playing" then direction = direction % 4 + 1
    else reset(); state = "playing" end
    draw()
end
