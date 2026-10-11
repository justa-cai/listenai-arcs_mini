-- 超级玛丽风格平台跳跃（SMB1 线性结构，纯色矩形绘制）
-- tile=12px，视口 20x20 格，横向卷轴。方向键移动（自动重复=按住），
-- A 键跳（按住跳更高），敌人踩踏，砖块顶撞，管道传送地下金币关。
-- 无手柄时：单击=左，双击=A 跳。矩形预算：只画非空格（天空跳过）。
do
    local C, W, H = ui.C, ui.W, ui.H
    local TS = 12                -- tile 像素
    local COLS = 20              -- 视口列数（240/12）
    local ROWS = 20              -- 视口行数（240/12，含 2 行 HUD/地面下沿）
    local HUD_H = 20             -- 顶部 HUD 高度（非 tile 区）
    local PLAY_ROWS = 15         -- 游玩区行数（HUD 下方）
    local GROUND_ROW = 15        -- 地面行索引（tile 坐标）
    local GRAV = 0.35            -- 重力 px/tick²
    local GRAV_HOLD = 0.18       -- 按住跳时的减重力
    local JUMP_V = -5.2          -- 跳跃初速
    local WALK_ACC = 0.18
    local WALK_MAX = 1.6
    local RUN_MAX = 2.4
    local ENEMY_SPD = 0.4
    local SHELL_SPD = 3.0
    local MAX_HOLD = 18          -- 按住跳的帧数上限

    -- 关卡字符图：# 地面  B 砖  ? 问号  H 硬砖  P 管道顶  p 管道身
    -- G Goomba  K Koopa  C 金币(空中)  S 无敌星  M 蘑菇(藏在?砖)
    -- F 旗杆  . 空
    local LEVELS = {
        { -- 第 1 关：教学（空旷→Goomba→?砖组→管道→坑→旗杆）
            cols = 110,
            ground = {{0, 68}, {72, 110}},
            pits = {{69, 71}},                   -- 坑（起列, 止列）
            items = {
                {c=16, r=11, t="B"}, {c=17, r=11, t="?"}, {c=18, r=11, t="B"},
                {c=22, r=7, t="B"}, {c=23, r=7, t="?"}, {c=24, r=7, t="B"},
                {c=23, r=11, t="M"},             -- 蘑菇藏在 ? 砖
                {c=20, r=12, t="G"},
                {c=28, r=12, t="P"}, {c=29, r=12, t="P"}, {c=28, r=13, t="p"}, {c=29, r=13, t="p"}, {c=28, r=14, t="p"}, {c=29, r=14, t="p"},
                {c=34, r=12, t="G"}, {c=36, r=12, t="G"},
                {c=40, r=11, t="B"}, {c=41, r=11, t="?"}, {c=42, r=11, t="B"},
                {c=44, r=7, t="B"}, {c=45, r=7, t="B"}, {c=46, r=7, t="B"},
                {c=48, r=12, t="K"},
                {c=55, r=11, t="?"}, {c=56, r=11, t="B"},
                {c=60, r=12, t="G"}, {c=62, r=12, t="G"},
                {c=76, r=12, t="P"}, {c=77, r=12, t="P"}, {c=76, r=13, t="p"}, {c=77, r=13, t="p"}, {c=76, r=14, t="p"}, {c=77, r=14, t="p"},
                {c=80, r=12, t="G"},
                {c=85, r=11, t="B"}, {c=86, r=11, t="S"},
                {c=90, r=12, t="F"},             -- 旗杆
                {c=95, r=10, t="H"}, {c=96, r=10, t="H"}, {c=97, r=10, t="H"},
                {c=95, r=11, t="H"}, {c=96, r=11, t="H"}, {c=97, r=11, t="H"},
            },
        },
        { -- 第 2 关：地下金币关（管道进入，出管道回到第 3 关）
            cols = 30,
            ground = {{0, 30}},
            pits = {},
            underground = true,
            items = {
                {c=5, r=10, t="C"}, {c=6, r=10, t="C"}, {c=7, r=10, t="C"},
                {c=10, r=8, t="C"}, {c=11, r=8, t="C"}, {c=12, r=8, t="C"}, {c=13, r=8, t="C"},
                {c=16, r=10, t="C"}, {c=17, r=10, t="C"}, {c=18, r=10, t="C"}, {c=19, r=10, t="C"},
                {c=14, r=11, t="B"}, {c=15, r=11, t="B"},
                {c=25, r=12, t="G"},
                {c=28, r=12, t="P"},             -- 出口管道
            },
        },
        { -- 第 3 关：挑战（更多 Koopa + 双坑 + 星砖）
            cols = 120,
            ground = {{0, 45}, {52, 90}, {98, 120}},
            pits = {{46, 51}, {91, 97}},
            items = {
                {c=8, r=11, t="?"}, {c=12, r=12, t="G"}, {c=14, r=12, t="K"},
                {c=18, r=7, t="B"}, {c=19, r=7, t="B"}, {c=20, r=7, t="?"}, {c=21, r=7, t="B"}, {c=22, r=7, t="B"},
                {c=20, r=11, t="B"}, {c=21, r=11, t="M"},
                {c=26, r=12, t="G"}, {c=28, r=12, t="G"}, {c=30, r=12, t="K"},
                {c=34, r=11, t="B"}, {c=35, r=11, t="?"}, {c=36, r=11, t="B"},
                {c=40, r=12, t="G"},
                {c=56, r=11, t="B"}, {c=57, r=11, t="B"}, {c=58, r=11, t="B"},
                {c=58, r=7, t="S"},
                {c=62, r=12, t="K"}, {c=64, r=12, t="G"},
                {c=70, r=7, t="B"}, {c=71, r=7, t="?"}, {c=72, r=7, t="B"},
                {c=76, r=12, t="G"}, {c=78, r=12, t="G"}, {c=80, r=12, t="G"},
                {c=100, r=12, t="F"},
                {c=105, r=10, t="H"}, {c=106, r=10, t="H"}, {c=107, r=10, t="H"}, {c=108, r=10, t="H"},
                {c=105, r=11, t="H"}, {c=106, r=11, t="H"}, {c=107, r=11, t="H"}, {c=108, r=11, t="H"},
            },
        },
    }

    -- 颜色
    local CLR = {
        sky     = 0x5C94FC,
        sky_u   = 0x000000,      -- 地下黑色
        ground  = 0xC84C0C,
        ground2 = 0xE45C10,
        brick   = 0xD07030,
        brickD  = 0xA05020,
        quest   = 0xFCA044,
        questH  = 0xE08020,
        hard    = 0x808080,
        pipe    = 0x00A800,
        pipeD   = 0x008000,
        marioS  = 0xFF0000,      -- 小马里奥红
        marioB  = 0xFF5050,      -- 大马里奥（更亮）
        goomba   = 0x884400,
        koopa   = 0x00CC00,
        shell   = 0x009900,
        coin    = 0xFFD700,
        mush    = 0xFF6060,
        star    = 0xFFFF00,
        flag    = 0x00FF00,
        hud     = 0x1C2A3A,
        hudTxt  = 0xFFFFFF,
        flash   = 0xFF69B4,      -- 无敌闪烁
    }

    -- 状态
    local lv_idx, lv, cam_x
    local tiles                                    -- [r][c] = tile 字符
    local enemies, coins_anim, items_anim          -- 动态实体
    local player
    local score, coin_count, lives, timer_val
    local state                                    -- ready|play|die|clear|over|win
    local state_t
    local key_left, key_right, key_jump = 0, 0, 0
    local jump_hold = 0
    local dirty = true

    local function refresh() dirty = true end

    -- ---------------------------------------------------------- 关卡加载
    local function load_level(idx, from_pipe)
        lv = LEVELS[idx]
        if not lv then state = "win"; state_t = 0; return end
        lv_idx = idx
        tiles = {}
        for r = 0, ROWS + 1 do
            tiles[r] = {}
        end
        -- 地面（跳过坑）
        local pits = lv.pits or {}
        local function is_pit(c)
            for _, p in ipairs(pits) do
                if c >= p[1] and c <= p[2] then return true end
            end
            return false
        end
        if lv.ground then
            for _, g in ipairs(lv.ground) do
                for c = g[1], g[2] do
                    if not is_pit(c) then
                        tiles[GROUND_ROW][c] = "#"
                        tiles[GROUND_ROW + 1][c] = "#"
                        tiles[GROUND_ROW + 2][c] = "#"
                    end
                end
            end
        end
        -- 物品
        for _, it in ipairs(lv.items) do
            tiles[it.r][it.c] = it.t
        end
        -- 实体
        enemies = {}
        for _, it in ipairs(lv.items) do
            if it.t == "G" then
                enemies[#enemies + 1] = {x = it.c * TS, y = (it.r - 1) * TS + HUD_H,
                                          w = TS - 1, h = TS - 1, kind = "goomba",
                                          vx = -ENEMY_SPD, dead = false, squash = 0}
                tiles[it.r][it.c] = nil
            elseif it.t == "K" then
                enemies[#enemies + 1] = {x = it.c * TS, y = (it.r - 1) * TS + HUD_H,
                                          w = TS - 1, h = TS - 1, kind = "koopa",
                                          vx = -ENEMY_SPD, shell = false, shell_vx = 0, squash = 0}
                tiles[it.r][it.c] = nil
            elseif it.t == "C" then
                tiles[it.r][it.c] = "C"
            end
        end
        -- 玩家
        local spawn_c = from_pipe and 2 or 2
        player = {
            x = spawn_c * TS, y = (GROUND_ROW - 2) * TS + HUD_H,
            vx = 0, vy = 0, w = TS - 2, h = TS - 2,
            big = false, star = 0, on_ground = false,
            facing = 1, anim = 0,
        }
        if from_pipe then player.big = from_pipe.big or false end
        cam_x = 0
        timer_val = lv.underground and 200 or 300
        state = from_pipe and "play" or "ready"
        state_t = 0
        refresh()
    end

    local function reset_game()
        score, coin_count, lives = 0, 0, 3
        load_level(1, nil)
    end

    -- ---------------------------------------------------------- tile 查询
    local function tile_at(c, r)
        if r < 0 or r > ROWS + 1 or c < 0 or c > (lv and lv.cols or 0) then
            return r >= GROUND_ROW and "#" or nil  -- 边界：底部视为地面
        end
        return tiles[r] and tiles[r][c] or nil
    end

    local function solid_tile(t)
        return t == "#" or t == "B" or t == "H" or t == "P" or t == "p" or t == "b"
    end

    -- 玩家 AABB vs tile 碰撞
    local function player_collide()
        local left = math.floor((player.x - player.w / 2) / TS)
        local right = math.floor((player.x + player.w / 2 - 1) / TS)
        local top = math.floor((player.y - player.h) / TS)
        local bot = math.floor((player.y - 1) / TS)
        -- 水平方向由调用方处理，这里只做垂直
        return left, right, top, bot
    end

    -- ---------------------------------------------------------- 物理 tick
    local function phys_step()
        if state ~= "play" then return end
        local p = player

        -- 计时
        timer_val = timer_val - 1 / 60
        if timer_val <= 0 then
            state = "die"; state_t = 0
            return
        end

        -- 水平
        local acc = WALK_ACC
        local max_v = key_jump and key_left == 0 and key_right == 0 and WALK_MAX or WALK_MAX
        if key_left > 0 then
            p.vx = p.vx - acc
            if p.vx < -max_v then p.vx = -max_v end
            p.facing = -1
        elseif key_right > 0 then
            p.vx = p.vx + acc
            if p.vx > max_v then p.vx = max_v end
            p.facing = 1
        else
            p.vx = p.vx * 0.85
            if math.abs(p.vx) < 0.05 then p.vx = 0 end
        end

        -- 垂直
        local g = GRAV
        if p.vy < 0 and key_jump > 0 and jump_hold < MAX_HOLD then
            g = GRAV_HOLD
            jump_hold = jump_hold + 1
        end
        p.vy = p.vy + g
        if p.vy > 4.5 then p.vy = 4.5 end

        -- 移动 + 碰撞（分轴）
        -- X 轴
        p.x = p.x + p.vx
        local pl = math.floor((p.x - p.w / 2) / TS)
        local pr = math.floor((p.x + p.w / 2 - 1) / TS)
        local pt = math.floor((p.y - p.h) / TS)
        local pb = math.floor((p.y - 1) / TS)
        if p.vx > 0 then
            for r = pt, pb do
                if solid_tile(tile_at(pr, r)) then
                    p.x = pr * TS - p.w / 2
                    p.vx = 0
                    break
                end
            end
        elseif p.vx < 0 then
            for r = pt, pb do
                if solid_tile(tile_at(pl, r)) then
                    p.x = (pl + 1) * TS + p.w / 2
                    p.vx = 0
                    break
                end
            end
        end

        -- Y 轴
        p.y = p.y + p.vy
        p.on_ground = false
        pl = math.floor((p.x - p.w / 2) / TS)
        pr = math.floor((p.x + p.w / 2 - 1) / TS)
        if p.vy > 0 then  -- 下落
            local foot_r = math.floor(p.y / TS)
            for c = pl, pr do
                local t = tile_at(c, foot_r)
                if solid_tile(t) then
                    p.y = foot_r * TS
                    p.vy = 0
                    p.on_ground = true
                    break
                end
            end
        elseif p.vy < 0 then  -- 上升：顶砖
            local head_r = math.floor((p.y - p.h) / TS)
            for c = pl, pr do
                local t = tile_at(c, head_r)
                if solid_tile(t) then
                    p.y = (head_r + 1) * TS + p.h
                    p.vy = 0
                    -- 顶砖效果
                    if t == "B" and p.big then
                        tiles[head_r][c] = nil       -- 大马里奥碎砖
                        score = score + 50
                    elseif t == "?" then
                        tiles[head_r][c] = "b"       -- 变空砖
                        -- ? 砖出什么：查原始 items 表
                        for _, it in ipairs(lv.items) do
                            if it.c == c and it.r == head_r then
                                if it.t == "M" then
                                    -- 弹蘑菇
                                    items_anim = items_anim or {}
                                    items_anim[#items_anim + 1] = {
                                        x = c * TS, y = head_r * TS - TS,
                                        kind = "mush", vx = 0.6, vy = -1,
                                        w = TS - 2, h = TS - 2, spawn = 30,
                                    }
                                elseif it.t == "S" then
                                    items_anim[#items_anim + 1] = {
                                        x = c * TS, y = head_r * TS - TS,
                                        kind = "star", vx = 0.8, vy = -1.5,
                                        w = TS - 2, h = TS - 2, spawn = 40,
                                    }
                                else
                                    coin_count = coin_count + 1
                                    score = score + 200
                                    if coin_count >= 100 then
                                        coin_count = coin_count - 100
                                        lives = lives + 1
                                    end
                                end
                                break
                            end
                        end
                    end
                    break
                end
            end
        end

        -- 落坑死亡
        if p.y > (GROUND_ROW + 4) * TS + HUD_H then
            state = "die"; state_t = 0
            return
        end

        -- 吃金币（空中）
        local pc = math.floor(p.x / TS)
        local pr2 = math.floor(p.y / TS)
        local pr3 = math.floor((p.y - p.h) / TS)
        for r = pr3, pr2 do
            if tile_at(pc, r) == "C" then
                tiles[r][pc] = nil
                coin_count = coin_count + 1
                score = score + 200
                if coin_count >= 100 then
                    coin_count = coin_count - 100
                    lives = lives + 1
                end
                ui.beep(2000, 20)
            end
        end

        -- 管道入口（站在 P 上按下）
        -- 简化：碰 P 顶时如果按下方向键（down），传送
        -- 出口管道：地下关碰 P 传回

        -- 旗杆
        local flag_c = nil
        for _, it in ipairs(lv.items) do
            if it.t == "F" then flag_c = it.c; break end
        end
        if flag_c and p.x > flag_c * TS then
            state = "clear"; state_t = 0
            score = score + math.max(0, math.floor(timer_val)) * 10
            return
        end

        -- 敌人更新
        for _, e in ipairs(enemies) do
            if not e.dead and e.squash == 0 then
                e.x = e.x + (e.shell and e.shell_vx or e.vx)
                -- 简化物理：不与 tile 碰撞，只走直线+掉坑
                e.y = e.y + 0.3
                local er = math.floor(e.y / TS)
                local ec = math.floor(e.x / TS)
                if solid_tile(tile_at(ec, er)) then
                    e.y = er * TS
                end
                -- 掉坑消失
                if e.y > (GROUND_ROW + 4) * TS then
                    e.dead = true
                end
                -- 壳撞墙反弹
                if e.shell and e.shell_vx ~= 0 then
                    local ec2 = e.shell_vx > 0 and math.floor((e.x + e.w) / TS)
                                                     or math.floor(e.x / TS)
                    if solid_tile(tile_at(ec2, math.floor(e.y / TS))) then
                        e.shell_vx = -e.shell_vx
                    end
                    -- 壳撞其他敌人
                    for _, e2 in ipairs(enemies) do
                        if e2 ~= e and not e2.dead and e2.squash == 0 then
                            if math.abs(e2.x - e.x) < TS and math.abs(e2.y - e.y) < TS then
                                e2.dead = true
                                score = score + 100
                            end
                        end
                    end
                end
                -- 玩家碰撞
                if math.abs(e.x - p.x) < TS and math.abs(e.y - p.y) < TS - 2 then
                    local stomped = p.vy > 0.5 and (p.y - p.h) < e.y - 4
                    if p.star > 0 then
                        e.dead = true
                        score = score + 200
                    elseif stomped then
                        if e.kind == "goomba" then
                            e.squash = 30
                            score = score + 100
                        elseif e.kind == "koopa" and not e.shell then
                            e.shell = true
                            e.shell_vx = 0
                            score = score + 100
                        elseif e.shell and e.shell_vx == 0 then
                            -- 踩壳：踢出
                            e.shell_vx = (p.x < e.x) and SHELL_SPD or -SHELL_SPD
                        end
                        p.vy = -3
                    elseif e.shell and e.shell_vx == 0 then
                        -- 侧碰静止壳：踢
                        e.shell_vx = (p.x < e.x) and SHELL_SPD or -SHELL_SPD
                    else
                        -- 受伤
                        if p.big then
                            p.big = false
                            p.star = 90  -- 受伤短暂无敌
                        else
                            state = "die"; state_t = 0
                            return
                        end
                    end
                end
            elseif e.squash > 0 then
                e.squash = e.squash - 1
                if e.squash == 0 then e.dead = true end
            end
        end

        -- 道具（蘑菇/星星）动画
        if items_anim then
            for i = #items_anim, 1, -1 do
                local it = items_anim[i]
                if it.spawn > 0 then
                    it.spawn = it.spawn - 1
                    it.y = it.y - 0.5
                else
                    it.x = it.x + it.vx
                    it.vy = it.vy + 0.3
                    it.y = it.y + it.vy
                    local r = math.floor(it.y / TS)
                    local c = math.floor(it.x / TS)
                    if solid_tile(tile_at(c, r)) then
                        it.y = r * TS
                        it.vy = 0
                    end
                    -- 碰到玩家
                    if math.abs(it.x - p.x) < TS and math.abs(it.y - p.y) < TS then
                        if it.kind == "mush" then
                            p.big = true
                            score = score + 1000
                        else
                            p.star = 300
                            score = score + 1000
                        end
                        table.remove(items_anim, i)
                    end
                    -- 超出关卡
                    if it.x < 0 or it.x > (lv.cols + 2) * TS then
                        table.remove(items_anim, i)
                    end
                end
            end
        end

        -- 无敌星计时
        if p.star > 0 then p.star = p.star - 1 end

        -- 摄像机
        local target = p.x - W / 2 + TS
        if target < 0 then target = 0 end
        local max_cam = (lv.cols - COLS) * TS
        if target > max_cam then target = max_cam end
        cam_x = cam_x + (target - cam_x) * 0.2
        if math.abs(cam_x - target) < 1 then cam_x = target end
    end

    -- ---------------------------------------------------------- 绘制
    local function tile_color(t, r)
        if t == "#" then return (r % 2 == 0) and CLR.ground or CLR.ground2
        elseif t == "B" then return CLR.brick
        elseif t == "b" then return CLR.brickD
        elseif t == "?" then return CLR.quest
        elseif t == "H" then return CLR.hard
        elseif t == "P" then return CLR.pipe
        elseif t == "p" then return CLR.pipeD
        elseif t == "C" then return CLR.coin
        end
        return CLR.sky
    end

    local function draw()
        screen.begin(lv and lv.underground and CLR.sky_u or CLR.sky)
        if not lv then
            screen.text("通关！", 80, 100, CLR.hudTxt)
            screen.text("得分 " .. score, 80, 130, CLR.hudTxt)
            screen.present()
            return
        end

        -- HUD
        screen.rect(0, 0, W, HUD_H, CLR.hud)
        screen.text("分" .. score, 4, 2, CLR.hudTxt)
        screen.text("币" .. coin_count, 74, 2, CLR.coin)
        screen.text("×" .. lives, 134, 2, CLR.hudTxt)
        screen.text("关" .. lv_idx .. (lv.underground and "地" or ""), 170, 2, CLR.hudTxt)
        screen.text("时" .. math.max(0, math.floor(timer_val)), 214, 2, CLR.hudTxt)

        -- tiles（只画视口内的非空）
        local c0 = math.floor(cam_x / TS)
        local c1 = c0 + COLS + 1
        for c = c0, c1 do
            for r = 0, ROWS do
                local t = tile_at(c, r)
                if t then
                    local x = c * TS - math.floor(cam_x)
                    local y = r * TS + HUD_H
                    if y >= HUD_H and y + TS <= H and x >= 0 and x + TS <= W then
                        screen.rect(x, y, TS, TS, tile_color(t, r))
                        -- 问号砖加中心点
                        if t == "?" then
                            screen.rect(x + 4, y + 4, 4, 4, CLR.questH)
                        elseif t == "C" then
                            screen.rect(x + 3, y + 2, 6, 8, CLR.coin)
                            screen.rect(x + 1, y + 3, 10, 6, CLR.coin)
                        elseif t == "F" then
                            screen.rect(x + 5, y, 2, TS, CLR.flag)
                        end
                    end
                end
            end
        end

        -- 敌人
        for _, e in ipairs(enemies) do
            if not e.dead and e.y < H + TS then
                local x = math.floor(e.x - cam_x)
                local ey = math.floor(e.y)
                if ey > H - TS then ey = H - TS end
                if ey < 0 then ey = 0 end
                if x >= 0 and x < W and ey >= 0 and ey < H then
                    if e.squash > 0 then
                        screen.rect(x, ey - 4, TS - 1, 6, CLR.goomba)
                    else
                        local col = e.kind == "goomba" and CLR.goomba or
                                    (e.shell and CLR.shell or CLR.koopa)
                        screen.rect(x, ey - TS + 1, TS - 1, TS - 1, col)
                        -- Goomba 眼睛
                        if e.kind == "goomba" then
                            screen.rect(x + 3, ey - TS + 4, 2, 2, 0xFFFFFF)
                            screen.rect(x + 7, ey - TS + 4, 2, 2, 0xFFFFFF)
                        end
                    end
                end
            end
        end

        -- 道具
        if items_anim then
            for _, it in ipairs(items_anim) do
                local x = math.floor(it.x - cam_x)
                local iy = math.floor(it.y)
                if iy > H then iy = H end
                if iy < 0 then iy = 0 end
                if x >= 0 and x < W and iy >= 0 and iy < H then
                    screen.rect(x, iy - TS, TS - 2, TS - 2,
                                it.kind == "mush" and CLR.mush or CLR.star)
                end
            end
        end

        -- 玩家
        if state ~= "clear" or true then
            local x = math.floor(player.x - player.w / 2 - cam_x)
            local y = math.floor(player.y - player.h)
            local ph = player.big and TS + 4 or TS - 2
            local pw = player.big and TS or TS - 2
            if y > H then y = H - ph end -- 死亡弹射出屏后钳制
            local col = player.star > 0 and
                        ((math.floor(player.star / 4) % 2 == 0) and CLR.flash or CLR.marioB) or
                        (player.big and CLR.marioB or CLR.marioS)
            screen.rect(x, y, pw, ph, col)
            -- 帽檐
            screen.rect(math.max(0, x - 1), y, pw + 2, 3, col)
            -- 眼睛（朝向）
            local ex = player.facing > 0 and x + pw - 5 or x + 3
            screen.rect(ex, y + 4, 2, 2, 0xFFFFFF)
        end

        -- 状态文字
        if state == "ready" then
            screen.text("第 " .. lv_idx .. " 关", 90, 100, CLR.hudTxt)
            screen.text("A键开始", 90, 120, CLR.hudTxt)
        elseif state == "die" then
            screen.text("死亡", 104, 100, CLR.hudTxt)
        elseif state == "clear" then
            screen.text("过关！", 96, 90, CLR.hudTxt)
            screen.text("+" .. math.max(0, math.floor(timer_val)) * 10 .. " 分", 84, 115, CLR.coin)
        elseif state == "over" then
            screen.text("游戏结束", 88, 90, CLR.hudTxt)
            screen.text("得分 " .. score, 92, 115, CLR.hudTxt)
            screen.text("A键重来", 92, 140, CLR.hudTxt)
        end

        screen.present()
    end

    -- ---------------------------------------------------------- 按键
    -- 方向键自动重复 = "按住"效果；用计数器衰减模拟松开
    local function tick_keys()
        if key_left > 0 then key_left = key_left - 1 end
        if key_right > 0 then key_right = key_right - 1 end
        if key_jump > 0 then key_jump = key_jump - 1 end
    end

    local function handle_key(id)
        if state == "ready" then
            if id == "function_double" or id == "function" then
                state = "play"; refresh()
            end
            return
        elseif state == "over" then
            if id == "function_double" or id == "function" then
                reset_game()
            end
            return
        elseif state ~= "play" then
            return
        end

        if id == "left" then
            key_left = 8               -- 8 tick 内视为按住（自动重复 ~每 2tick 补）
        elseif id == "right" then
            key_right = 8
        elseif id == "function_double" or id == "up" then
            if player.on_ground then
                key_jump = 10
                jump_hold = 0
                player.vy = JUMP_V
                player.on_ground = false
            end
        elseif id == "function" then
            -- 无手柄时单击=左移一步
            key_left = 6
        elseif id == "down" then
            -- 管道入口（站管顶按下）：检查脚下
            local pc = math.floor(player.x / TS)
            local pr = math.floor(player.y / TS)
            if tile_at(pc, pr) == "P" then
                if lv.underground then
                    -- 地下出口：回第 3 关
                    local saved_big = player.big
                    local saved_score, saved_coins, saved_lives = score, coin_count, lives
                    load_level(3, {big = saved_big})
                    score, coin_count, lives = saved_score, saved_coins, saved_lives
                else
                    -- 入口：去地下金币关
                    load_level(2, {big = player.big})
                    score = score -- 保持
                end
            end
        end
    end

    -- ---------------------------------------------------------- 状态机
    local function state_step()
        if state == "die" then
            state_t = state_t + 1
            player.vy = player.vy + 0.3
            player.y = player.y + player.vy
            if state_t > 90 then
                lives = lives - 1
                if lives <= 0 then
                    state = "over"; state_t = 0
                else
                    local idx = lv_idx
                    if lv.underground then idx = 1 end
                    load_level(idx, nil)
                end
            end
        elseif state == "clear" then
            state_t = state_t + 1
            if state_t > 120 then
                local next_idx = lv.underground and 3 or (lv_idx + 1)
                if next_idx > 3 then
                    state = "win"
                else
                    local sb, sc, sc_, sl = player.big, score, coin_count, lives
                    load_level(next_idx, {big = sb})
                    score, coin_count, lives = sc, sc_, sl
                end
            end
        end
    end

    -- ---------------------------------------------------------- 注册
    ui.register({
        id = "mario",
        name = "超级玛丽",
        icon = "sokoban",            -- 复用现有图标
        color = CLR.marioS,
        enter = function(self)
            if not player then reset_game() end
            refresh()
        end,
        tick = function(self, dt)
            if state == "play" then
                tick_keys()
                phys_step()
                refresh()
            elseif state == "die" or state == "clear" then
                state_step()
                refresh()
            elseif state == "ready" or state == "over" then
                -- 静态画面
            end
        end,
        draw = function(self) draw() end,
        key = function(self, id)
            handle_key(id)
            return true                  -- 游戏独占全部按键
        end,
        leave = function(self)
            key_left, key_right, key_jump = 0, 0, 0
        end,
    })
end
