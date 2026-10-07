-- Flappy：A / 上键拍翅（无手柄时单击、双击都算拍一次）。撞管或落地结束。
do
    local C, W, H = ui.C, ui.W, ui.H
    local FLOOR = H - 30
    local BX, BR = 58, 10
    local PIPE_W, GAP, SPEED = 34, 76, 3
    local GRAVITY, FLAP = 0.62, -6.2

    local bird, vy, pipes, score, best, state, acc, spawn_acc

    local function new_pipe()
        local top = 40 + math.random(0, math.max(1, FLOOR - 40 - GAP - 40))
        return {x = W, top = top, scored = false}
    end

    local function reset()
        bird, vy, pipes, score, acc, spawn_acc = 120, 0, {}, 0, 0, 0
        pipes[1] = new_pipe()
        pipes[1].x = W + 40
        state = "ready"
    end

    local function hit_pipe(p)
        if BX + BR < p.x or BX - BR > p.x + PIPE_W then return false end
        return (bird - BR) < p.top or (bird + BR) > (p.top + GAP)
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
        ui.begin("Flappy")
        ui.text(string.format("得分 %d   最高 %d", score, best), 12, 32, C.text)

        local keep = {}
        for _, p in ipairs(pipes) do
            if p.x + PIPE_W > 0 then
                keep[#keep + 1] = p
                -- 管道生成在 x=W 再向左滑入, 消失时右缘还会探进屏幕 —— 两头都得裁。
                -- 裁剪不是"为了好看": 固件对 rect 做的是 x∈[0,239] 的范围检查,
                -- 越界直接 luaL_error 打掉整个实例(表现为"玩着玩着忽然退出桌面")。
                -- 只裁右边的话, 管道滑出左缘(x 变成负数)那一拍就会炸。
                local px = math.floor(p.x)
                local left = math.max(0, px)
                local right = math.min(W, px + PIPE_W)
                local w = right - left
                if w > 0 then
                    screen.rect(left, 28, w, p.top - 28, C.hi)
                    screen.rect(left, p.top + GAP, w, FLOOR - (p.top + GAP), C.hi)
                end
            end
        end
        pipes = keep

        screen.rect(0, FLOOR, W, H - FLOOR, C.line)
        screen.rect(math.floor(BX - BR), math.floor(bird - BR), BR * 2, BR * 2,
                    state == "over" and C.bad or C.warn)
        menu()
    end

    ui.register({
        id = "flappy",
        name = "Flappy",
        icon = "flappy",
        color = C.hi,
        raw = true,
        enter = function(self)
            math.randomseed(ui.ms() + 7)
            best = ui.load("flappy.best", 0)
            if type(best) ~= "number" then best = 0 end
            reset()
            ui.refresh()
        end,
        tick = function(self, dt)
            if state ~= "playing" then return end
            acc = acc + dt
            while acc >= 20 do
                acc = acc - 20
                vy = vy + GRAVITY
                bird = bird + vy
                if bird < 30 or bird > FLOOR - BR then state = "over"; break end
                for _, p in ipairs(pipes) do
                    p.x = p.x - SPEED
                    if not p.scored and p.x + PIPE_W < BX - BR then
                        p.scored = true
                        score = score + 1
                        if score > best then
                            best = score
                            ui.save("flappy.best", best)
                        end
                        ui.beep(1600, 25)
                    end
                    if hit_pipe(p) then state = "over"; break end
                end
                if state == "over" then break end
                spawn_acc = spawn_acc + 1
                if spawn_acc >= 46 then
                    spawn_acc = 0
                    pipes[#pipes + 1] = new_pipe()
                end
            end
            ui.refresh()
        end,
        draw = function(self) draw() end,
        click = function(self, id)
            if state ~= "playing" then return end
            -- 只有"拍翅"键算数: 手柄上=A, 没有手柄时=单击/双击
            if id ~= "up" and id ~= "function" and id ~= "function_double" then return end
            vy = FLAP
            ui.beep(1000, 20)
            ui.refresh()
        end,
        leave = function(self) end,
    })
end
