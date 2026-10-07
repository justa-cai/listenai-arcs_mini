-- 打砖块：挡板自动往返，左右键把它推向对应方向；无手柄时单击 = 掉头。
do
    local C, W, H = ui.C, ui.W, ui.H
    local COLS, ROWS = 6, 4
    local BW, BH, BX0, BY0, BGAP = 36, 13, 6, 42, 3
    local FLOOR = H - 30
    local PW, PH = 46, 6
    local PADDLE_SPEED, BALL_SPEED = 2.4, 2.6

    local bricks, paddle_x, paddle_dir, ball, score, best, state, acc

    local function reset()
        bricks = {}
        for r = 1, ROWS do
            for c = 1, COLS do
                bricks[#bricks + 1] = {c = c, r = r, alive = true}
            end
        end
        paddle_x, paddle_dir = (W - PW) / 2, 1
        ball = {x = W / 2, y = FLOOR - 40, vx = 1.0 * BALL_SPEED, vy = -1.0 * BALL_SPEED}
        score, acc, state = 0, 0, "ready"
    end

    local function alive_count()
        local n = 0
        for _, b in ipairs(bricks) do if b.alive then n = n + 1 end end
        return n
    end

    local function brick_rect(b)
        return BX0 + (b.c - 1) * (BW + BGAP), BY0 + (b.r - 1) * (BH + BGAP), BW, BH
    end

    local function menu()
        if state == "playing" then ui.commands_clear(); return end
        ui.commands({
            { label = "返回", run = ui.back },
            { label = (state == "ready") and "开始" or "重开",
              run = function() reset(); state = "playing"; ui.refresh() end },
        })
    end

    local function draw()
        ui.begin("打砖块")
        ui.text(string.format("得分 %d   最高 %d", score, best), 12, 32, C.text)

        for _, b in ipairs(bricks) do
            if b.alive then
                local x, y, w, h = brick_rect(b)
                screen.rect(x, y, w, h, (b.r % 2 == 0) and C.hi or C.warn)
            end
        end
        screen.rect(math.floor(paddle_x), FLOOR - PH, PW, PH, C.text)
        screen.rect(math.floor(ball.x) - 3, math.floor(ball.y) - 3, 6, 6, C.text)
        screen.rect(0, FLOOR, W, H - FLOOR, C.line)
        menu()
    end

    local function ball_hits_paddle()
        return ball.y + 3 >= FLOOR - PH and ball.y - 3 <= FLOOR
            and ball.x >= paddle_x - 3 and ball.x <= paddle_x + PW + 3
    end

    ui.register({
        id = "brick",
        name = "打砖块",
        icon = "brick",
        color = C.warn,
        raw = true,
        enter = function(self)
            best = ui.load("brick.best", 0)
            if type(best) ~= "number" then best = 0 end
            reset()
            ui.refresh()
        end,
        tick = function(self, dt)
            if state ~= "playing" then return end
            acc = acc + dt
            while acc >= 20 do
                acc = acc - 20

                paddle_x = paddle_x + paddle_dir * PADDLE_SPEED
                if paddle_x <= 2 then paddle_x, paddle_dir = 2, 1 end
                if paddle_x + PW >= W - 2 then paddle_x, paddle_dir = W - 2 - PW, -1 end

                ball.x = ball.x + ball.vx
                ball.y = ball.y + ball.vy
                if ball.x <= 4 then ball.x, ball.vx = 4, -ball.vx end
                if ball.x >= W - 4 then ball.x, ball.vx = W - 4, -ball.vx end
                if ball.y <= 30 then ball.y, ball.vy = 30, -ball.vy end
                if ball.y >= FLOOR + 10 then state = "over"; break end

                if ball.vy > 0 and ball_hits_paddle() then
                    ball.y = FLOOR - PH - 4
                    ball.vy = -math.abs(ball.vy)
                    ui.beep(1200, 25)
                end

                for _, b in ipairs(bricks) do
                    if b.alive then
                        local x, y, w, h = brick_rect(b)
                        if ball.x + 3 > x and ball.x - 3 < x + w
                           and ball.y + 3 > y and ball.y - 3 < y + h then
                            b.alive = false
                            ball.vy = -ball.vy
                            score = score + 1
                            if score > best then
                                best = score
                                ui.save("brick.best", best)
                            end
                            ui.beep(1500, 25)
                            break
                        end
                    end
                end
                if alive_count() == 0 then state = "won"; break end
            end
            ui.refresh()
        end,
        draw = function(self) draw() end,
        click = function(self, id)
            if state ~= "playing" then return end
            -- 手柄左右键: 直接把往返方向定成你要的方向 (按住 = 持续推)
            if id == "left" then
                paddle_dir = -1
            elseif id == "right" then
                paddle_dir = 1
            elseif id == "up" or id == "function" or id == "function_double" then
                paddle_dir = -paddle_dir          -- 没有手柄/上键 = 掉头
            end
            ui.refresh()
        end,
        leave = function(self) end,
    })
end
