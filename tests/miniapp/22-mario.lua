-- 超级玛丽风格平台跳跃（SMB1 线性结构，纯色矩形分层绘制）
-- tile=12px，视口 20x20 格，横向卷轴。方向键移动（自动重复=按住），
-- A 键跳（按住跳更高），敌人踩踏，砖块顶撞，管道传送地下金币关。
-- 视觉：分块精灵（帽子/脸/背带裤/鞋）、砖缝纹理、管道高光、
--       云/山丘/灌木背景（视差）、过关旗、SMB 式黑屏关卡报幕。
-- 无手柄时：单击=左，双击=A 跳。矩形预算 126/帧（装饰层自动降级）。

-- 独立运行壳层（设备端无桌面 ui 表）
local C = { bg = 0x101923, bar = 0x1C2A3A, text = 0xFFFFFF,
            hi = 0x7E9CFF, dim = 0xA5B5C5, warn = 0xFFB84D, line = 0x334455 }
local W, H = 240, 240
local _saved = {}
local _app = nil
-- rect 钳制包装 + 计数：任何越界参数拉回屏幕内；126 硬顶防超限崩溃
local rc = 0
local _raw_rect = screen.rect
screen.rect = function(x, y, w, h, c)
    if rc >= 126 then return end
    if x < 0 then w = w + x; x = 0 end
    if y < 0 then h = h + y; y = 0 end
    if x + w > 240 then w = 240 - x end
    if y + h > 240 then h = 240 - y end
    if w > 0 and h > 0 then rc = rc + 1; _raw_rect(x, y, w, h, c) end
end

ui = ui or {
    W = W, H = H, C = C,
    HAS_BUZZER = (type(buzzer) == "table"),
    beep = function(hz, ms) if buzzer then buzzer.play(hz, ms) end end,
    save = function(k, v) _saved[k] = v end,
    load = function(k) return _saved[k] end,
    commands = function() end,
    commands_clear = function() end,
    text = function(s, x, y, c) screen.text(s, x, y, c) end,
    text_center = function(s, y, c) screen.text(s, 100, y, c) end,
    begin = function(t) end,
    footer = function(s) end,
    clip = function(s) return s end,
    register = function(def) _app = def end,
    back = function() end,
}

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

    -- 调色板（NES SMB1 取色）
    local PAL = {
        sky     = 0x5C94FC,
        gBase   = 0xC84C0C,   -- 地面砖基色
        gTop    = 0xFC9838,   -- 地表亮层
        gMortar = 0x381400,   -- 砖缝
        brick   = 0xC84C0C,
        mortar  = 0x381400,
        qBase   = {0xFC9838, 0xF8B050, 0xE08428},   -- ? 砖呼吸三档
        qLight  = 0xFFD9A0,
        qDark   = 0xA04808,
        hard    = 0x9C9C9C, hardL = 0xD8D8D8, hardD = 0x585858,
        pipe    = 0x00A800, pipeL = 0x80D010, pipeD = 0x005800,
        mRed    = 0xF83800, mSkin = 0xFCA044, mOvl = 0x0058F8,
        mShoe   = 0x7C2D00, mEye = 0x18100C,
        gmbA    = 0x9C4A00, gmbB = 0xC86A20,
        kShell  = 0x00A800, kShellL = 0x80D010, kSkin = 0xFCD8A8,
        coin    = 0xF8B800, coinL = 0xFFE07A,
        mush    = 0xF83800, star = 0xF8B800,
        flagG   = 0x00A800, flagD = 0x005800,
        cloud   = 0xFCFCFC, cloudSh = 0xA8CCF8,
        hill    = 0x00A800, hillD = 0x005800,
        hudBg   = 0x101923, hudSep = 0x2E4258,
    }
    local UG = {                 -- 地下关：蓝砖 + 黑天
        sky     = 0x000000,
        gBase   = 0x2860D8, gTop = 0x5A9CFF, gMortar = 0x0A1230,
        brick   = 0x2860D8, mortar = 0x0A1230,
        hard    = 0x9C9C9C, hardL = 0xD8D8D8, hardD = 0x585858,
    }
    local STARC = { 0xF83800, 0xF8B800, 0x00B800, 0xFCFCFC }

    -- 状态
    local lv_idx, lv, cam_x
    local camx, P                          -- 帧内缓存：取整摄像机 / 当前调色板
    local tiles                                    -- [r][c] = tile 字符
    local enemies, coins_anim, items_anim          -- 动态实体
    local player
    local score, coin_count, lives, timer_val
    local state                                    -- ready|play|die|clear|over|win
    local state_t
    local gtime = 0                                -- 全局动画时钟
    local flag_c
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
        flag_c = nil
        for _, it in ipairs(lv.items) do
            if it.t == "F" then flag_c = it.c; break end
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
        -- M/S 是藏道具的 ? 砖：必须可顶（旧版漏掉导致蘑菇/星星永远拿不到）
        return t == "#" or t == "B" or t == "H" or t == "P" or t == "p"
               or t == "b" or t == "?" or t == "M" or t == "S"
    end

    local function is_quest(t)
        return t == "?" or t == "M" or t == "S"
    end

    -- ---------------------------------------------------------- 物理 tick
    local function phys_step()
        if state ~= "play" then return end
        local p = player

        -- 计时
        timer_val = timer_val - 1 / 60
        if timer_val <= 0 then
            state = "die"; state_t = 0; p.vy = -4
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

        -- 移动 + 碰撞（分轴）。tile 行号必须扣除 HUD 偏移：
        -- 屏幕 y = 行*TS + HUD_H，直接 y/TS 会悬空 8~20px（旧版顽疾）
        p.x = p.x + p.vx
        local pl = math.floor((p.x - p.w / 2) / TS)
        local pr = math.floor((p.x + p.w / 2 - 1) / TS)
        local pt = math.floor((p.y - p.h - HUD_H) / TS)
        local pb = math.floor((p.y - 1 - HUD_H) / TS)
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

        p.y = p.y + p.vy
        p.on_ground = false
        pl = math.floor((p.x - p.w / 2) / TS)
        pr = math.floor((p.x + p.w / 2 - 1) / TS)
        if p.vy > 0 then  -- 下落
            local foot_r = math.floor((p.y - HUD_H) / TS)
            for c = pl, pr do
                local t = tile_at(c, foot_r)
                if solid_tile(t) then
                    p.y = foot_r * TS + HUD_H
                    p.vy = 0
                    p.on_ground = true
                    break
                end
            end
        elseif p.vy < 0 then  -- 上升：顶砖
            local head_r = math.floor((p.y - p.h - HUD_H) / TS)
            for c = pl, pr do
                local t = tile_at(c, head_r)
                if solid_tile(t) then
                    p.y = (head_r + 1) * TS + HUD_H + p.h
                    p.vy = 0
                    -- 顶砖效果
                    if t == "B" and p.big then
                        tiles[head_r][c] = nil       -- 大马里奥碎砖
                        score = score + 50
                        ui.beep(240, 30)
                    elseif is_quest(t) then
                        tiles[head_r][c] = "b"       -- 变空砖
                        -- ? 砖出什么：查原始 items 表
                        for _, it in ipairs(lv.items) do
                            if it.c == c and it.r == head_r then
                                if it.t == "M" then
                                    -- 弹蘑菇
                                    items_anim = items_anim or {}
                                    items_anim[#items_anim + 1] = {
                                        x = c * TS, y = head_r * TS + HUD_H,
                                        kind = "mush", vx = 0.6, vy = -1,
                                        w = TS - 2, h = TS - 2, spawn = 30,
                                    }
                                    ui.beep(1300, 60)
                                elseif it.t == "S" then
                                    items_anim[#items_anim + 1] = {
                                        x = c * TS, y = head_r * TS + HUD_H,
                                        kind = "star", vx = 0.8, vy = -1.5,
                                        w = TS - 2, h = TS - 2, spawn = 40,
                                    }
                                    ui.beep(1600, 60)
                                else
                                    coin_count = coin_count + 1
                                    score = score + 200
                                    if coin_count >= 100 then
                                        coin_count = coin_count - 100
                                        lives = lives + 1
                                    end
                                    ui.beep(1900, 20)
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
        local pr2 = math.floor((p.y - HUD_H) / TS)
        local pr3 = math.floor((p.y - p.h - HUD_H) / TS)
        for r = pr3, pr2 do
            if tile_at(pc, r) == "C" then
                tiles[r][pc] = nil
                coin_count = coin_count + 1
                score = score + 200
                if coin_count >= 100 then
                    coin_count = coin_count - 100
                    lives = lives + 1
                end
                ui.beep(1900, 20)
            end
        end

        -- 旗杆
        if flag_c and p.x > flag_c * TS then
            state = "clear"; state_t = 0
            score = score + math.max(0, math.floor(timer_val)) * 10
            ui.beep(880, 100)
            return
        end

        -- 敌人更新
        for _, e in ipairs(enemies) do
            if not e.dead and e.squash == 0 then
                e.x = e.x + (e.shell and e.shell_vx or e.vx)
                -- 垂直：落到 tile 表面（行号同样扣 HUD 偏移）
                e.y = e.y + 0.3
                local er = math.floor((e.y - HUD_H) / TS)
                local ec = math.floor(e.x / TS)
                if solid_tile(tile_at(ec, er)) then
                    e.y = er * TS + HUD_H
                end
                -- 掉坑消失
                if e.y > (GROUND_ROW + 4) * TS + HUD_H then
                    e.dead = true
                end
                -- 行走遇墙折返（壳走自己的反弹逻辑）
                if not e.shell then
                    local mid_r = math.floor((e.y - 6 - HUD_H) / TS)
                    local nc = e.vx > 0 and math.floor((e.x + e.w + 1) / TS)
                                             or math.floor((e.x - 1) / TS)
                    if solid_tile(tile_at(nc, mid_r)) then
                        e.vx = -e.vx
                    end
                end
                -- 壳撞墙反弹
                if e.shell and e.shell_vx ~= 0 then
                    local ec2 = e.shell_vx > 0 and math.floor((e.x + e.w) / TS)
                                                     or math.floor(e.x / TS)
                    if solid_tile(tile_at(ec2, math.floor((e.y - HUD_H) / TS))) then
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
                        ui.beep(500, 30)
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
                        ui.beep(220, 25)
                    elseif e.shell and e.shell_vx == 0 then
                        -- 侧碰静止壳：踢
                        e.shell_vx = (p.x < e.x) and SHELL_SPD or -SHELL_SPD
                    else
                        -- 受伤
                        if p.big then
                            p.big = false
                            p.star = 90  -- 受伤短暂无敌
                            ui.beep(160, 60)
                        else
                            state = "die"; state_t = 0; p.vy = -4
                            ui.beep(140, 150)
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
                    local r = math.floor((it.y - HUD_H) / TS)
                    local c = math.floor(it.x / TS)
                    if solid_tile(tile_at(c, r)) then
                        it.y = r * TS + HUD_H
                        it.vy = 0
                    end
                    -- 碰到玩家
                    if math.abs(it.x - p.x) < TS and math.abs(it.y - p.y) < TS then
                        if it.kind == "mush" then
                            p.big = true
                            score = score + 1000
                            ui.beep(1000, 80)
                        else
                            p.star = 300
                            score = score + 1000
                            ui.beep(1400, 80)
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
        p.anim = p.anim + math.abs(p.vx)

        -- 摄像机
        local target = p.x - W / 2 + TS
        if target < 0 then target = 0 end
        local max_cam = (lv.cols - COLS) * TS
        if target > max_cam then target = max_cam end
        cam_x = cam_x + (target - cam_x) * 0.2
        if math.abs(cam_x - target) < 1 then cam_x = target end
    end

    -- ---------------------------------------------------------- 绘制原语
    -- 装饰层：预算封顶（26），超了自动降级，保证实体层永远画得下
    local decor = 0
    local function D(x, y, w, h, c)
        if decor >= 26 then return end
        decor = decor + 1
        screen.rect(x, y, w, h, c)
    end

    local function draw_hud()
        screen.rect(0, 0, W, HUD_H, PAL.hudBg)
        screen.rect(0, HUD_H - 1, W, 1, PAL.hudSep)
        screen.text(string.format("分%06d", score), 2, 2, 0xFFFFFF)
        -- 金币图标
        screen.rect(80, 5, 4, 9, PAL.coin)
        screen.rect(81, 6, 1, 7, PAL.coinL)
        screen.text(string.format("×%02d", coin_count), 87, 2, PAL.coin)
        -- 命数图标（小马里奥头）
        screen.rect(122, 3, 8, 4, PAL.mRed)
        screen.rect(124, 7, 5, 5, PAL.mSkin)
        screen.text("×" .. lives, 133, 2, 0xFFFFFF)
        screen.text("关" .. lv_idx .. (lv.underground and "B" or ""), 154, 2, 0xFFFFFF)
        local tcol = timer_val < 60 and 0xFF5050 or 0xFFFFFF
        screen.text(string.format("时%03d", math.max(0, math.floor(timer_val))), 194, 2, tcol)
    end

    -- 小/大马里奥。pose: 0立 1走A 2走B 3跳
    local function draw_mario(x, y, big, facing, star_left, pose)
        local cap, ovl
        if star_left > 0 then
            local i = math.floor(gtime / 3) % 4 + 1
            cap = STARC[i]
            ovl = STARC[i % 4 + 1]
        else
            cap = PAL.mRed
            ovl = PAL.mOvl
        end
        local skin, shoe, eye = PAL.mSkin, PAL.mShoe, PAL.mEye
        if big then
            screen.rect(x, y, 12, 4, cap)
            screen.rect(x + 2, y + 4, 9, 4, skin)
            screen.rect(facing > 0 and x + 7 or x + 3, y + 5, 2, 2, eye)
            screen.rect(x, y + 8, 12, 4, cap)
            screen.rect(x + 3, y + 8, 6, 4, ovl)
            screen.rect(x + 1, y + 12, 4, 2, ovl)
            screen.rect(x + 7, y + 12, 4, 2, ovl)
            if pose == 3 then
                screen.rect(x + 3, y + 14, 6, 2, shoe)
            elseif pose == 1 then
                screen.rect(x - 1, y + 14, 5, 2, shoe)
                screen.rect(x + 8, y + 14, 5, 2, shoe)
            else
                screen.rect(x, y + 14, 5, 2, shoe)
                screen.rect(x + 7, y + 14, 5, 2, shoe)
            end
        else
            screen.rect(x, y, 12, 3, cap)
            screen.rect(x + 2, y + 3, 9, 3, skin)
            screen.rect(facing > 0 and x + 7 or x + 3, y + 3, 2, 2, eye)
            screen.rect(x, y + 6, 12, 3, cap)
            screen.rect(x + 3, y + 6, 6, 3, ovl)
            if pose == 3 then
                screen.rect(x + 4, y + 9, 5, 3, shoe)
            elseif pose == 1 then
                screen.rect(x - 1, y + 9, 5, 3, shoe)
                screen.rect(x + 8, y + 9, 4, 3, shoe)
            else
                screen.rect(x, y + 9, 5, 3, shoe)
                screen.rect(x + 7, y + 9, 5, 3, shoe)
            end
        end
    end

    local function draw_goomba(x, feet, squash)
        if squash > 0 then
            screen.rect(x, feet - 4, 12, 4, PAL.gmbB)
            screen.rect(x + 2, feet - 3, 3, 2, 0xFCFCFC)
            screen.rect(x + 7, feet - 3, 3, 2, 0xFCFCFC)
            return
        end
        screen.rect(x + 1, feet - 12, 10, 4, PAL.gmbA)   -- 头顶
        screen.rect(x, feet - 9, 12, 5, PAL.gmbB)        -- 脸
        screen.rect(x + 2, feet - 8, 3, 3, 0xFCFCFC)     -- 眼白
        screen.rect(x + 7, feet - 8, 3, 3, 0xFCFCFC)
        screen.rect(x + 3, feet - 7, 2, 2, 0x18100C)     -- 瞳
        screen.rect(x + 7, feet - 7, 2, 2, 0x18100C)
        if math.floor((gtime + x) / 8) % 2 == 0 then     -- 走路摆脚
            screen.rect(x - 1, feet - 3, 5, 3, 0x18100C)
            screen.rect(x + 8, feet - 3, 5, 3, 0x18100C)
        else
            screen.rect(x, feet - 3, 5, 3, 0x18100C)
            screen.rect(x + 7, feet - 3, 5, 3, 0x18100C)
        end
    end

    local function draw_koopa(x, feet, facing, shell, moving)
        if shell then
            screen.rect(x + 1, feet - 8, 10, 8, PAL.kShell)
            if moving then
                local sx = x + 2 + (math.floor(gtime / 3) % 3) * 2
                screen.rect(sx, feet - 7, 4, 3, PAL.kShellL)
            else
                screen.rect(x + 3, feet - 7, 5, 3, PAL.kShellL)
            end
            screen.rect(x + 2, feet - 2, 8, 1, 0xFCE0A8)
            return
        end
        local top = feet - 14
        if facing > 0 then
            screen.rect(x + 8, top, 4, 5, PAL.kSkin)
            screen.rect(x + 10, top + 1, 2, 2, 0x18100C)
        else
            screen.rect(x, top, 4, 5, PAL.kSkin)
            screen.rect(x, top + 1, 2, 2, 0x18100C)
        end
        screen.rect(x, top + 5, 12, 7, PAL.kShell)
        screen.rect(x + 2, top + 6, 8, 3, PAL.kShellL)
        screen.rect(x + 1, top + 11, 10, 1, 0xFCE0A8)
        if math.floor((gtime + x) / 8) % 2 == 0 then
            screen.rect(x + 2, feet - 2, 4, 2, PAL.kSkin)
            screen.rect(x + 7, feet - 2, 4, 2, PAL.kSkin)
        else
            screen.rect(x + 1, feet - 2, 4, 2, PAL.kSkin)
            screen.rect(x + 6, feet - 2, 4, 2, PAL.kSkin)
        end
    end

    local function draw_item(it, x)
        local feet = math.floor(it.y)
        if it.kind == "mush" then
            screen.rect(x + 1, feet - 12, 10, 7, PAL.mush)
            screen.rect(x + 2, feet - 11, 3, 2, 0xFCFCFC)
            screen.rect(x + 7, feet - 10, 3, 2, 0xFCFCFC)
            screen.rect(x + 3, feet - 5, 6, 5, 0xFCD8A8)
        else
            local c = math.floor(gtime / 4) % 2 == 0 and PAL.star or 0xFCFCFC
            screen.rect(x + 5, feet - 12, 2, 2, c)
            screen.rect(x + 2, feet - 10, 8, 7, c)
            screen.rect(x + 1, feet - 6, 3, 4, c)
            screen.rect(x + 8, feet - 6, 3, 4, c)
            screen.rect(x + 3, feet - 9, 2, 2, 0x18100C)
            screen.rect(x + 7, feet - 9, 2, 2, 0x18100C)
        end
    end

    -- ---------------------------------------------------------- 场景分层
    local function draw_scenery(gy)
        if lv.underground then
            -- 地下：天花板砖
            screen.rect(0, HUD_H, W, 5, P.gBase)
            local off = camx % 24
            for x = -off + 12, W, 24 do
                D(x, HUD_H, 1, 5, P.gMortar)
            end
            return
        end
        -- 山丘（贴地，1.0 视差）
        local hspan = 21 * TS
        for k = math.floor((camx - 40) / hspan), math.floor((camx + W + 40) / hspan) do
            local hx = math.floor(k * hspan + 2 * TS - camx)
            D(hx + 9, gy - 15, 12, 6, PAL.hill)
            D(hx + 12, gy - 18, 6, 3, PAL.hill)
            D(hx, gy - 9, 30, 9, PAL.hill)
            D(hx + 6, gy - 6, 2, 2, PAL.hillD)
            D(hx + 20, gy - 5, 2, 2, PAL.hillD)
        end
        -- 灌木
        local bspan = 13 * TS
        for k = math.floor((camx - 40) / bspan), math.floor((camx + W + 40) / bspan) do
            local bx = math.floor(k * bspan + 7 * TS - camx)
            D(bx, gy - 6, 26, 6, PAL.hill)
            D(bx + 4, gy - 8, 8, 2, PAL.hill)
            D(bx + 15, gy - 8, 7, 2, PAL.hill)
        end
        -- 云（0.5 视差）
        local px = camx * 0.5
        local cspan = 17 * TS
        for k = math.floor((px - 40) / cspan), math.floor((px + W + 40) / cspan) do
            local cy = 46 + (k % 3) * 20
            local cx = math.floor(k * cspan + 4 * TS - px)
            D(cx + 3, cy - 3, 8, 3, PAL.cloud)
            D(cx + 12, cy - 2, 8, 2, PAL.cloud)
            D(cx, cy, 22, 7, PAL.cloud)
            D(cx + 2, cy + 7, 18, 2, PAL.cloudSh)
            local cx2 = math.floor(k * cspan + 11 * TS - px)
            D(cx2 + 2, cy + 10, 8, 2, PAL.cloud)
            D(cx2, cy + 12, 16, 6, PAL.cloud)
        end
    end

    local function draw_ground(gy, c0, c1)
        local c = c0
        while c <= c1 do
            if tile_at(c, GROUND_ROW) == "#" then
                local s = c
                while c <= c1 and tile_at(c, GROUND_ROW) == "#" do
                    c = c + 1
                end
                local x0 = math.max(0, s * TS - camx)
                local x1 = math.min(W, c * TS - camx)
                if x1 > x0 then
                    -- 主体一直铺到屏幕底（旧版 236~240 露天色）
                    screen.rect(x0, gy, x1 - x0, H - gy, P.gBase)
                    screen.rect(x0, gy, x1 - x0, 2, P.gTop)
                    screen.rect(x0, gy + TS, x1 - x0, 1, P.gMortar)
                    screen.rect(x0, gy + 2 * TS, x1 - x0, 1, P.gMortar)
                    for tc = s, c - 1 do        -- 竖砖缝（装饰）
                        D(tc * TS - camx + TS - 1, gy + 3, 1, TS - 3, P.gMortar)
                    end
                end
            else
                c = c + 1
            end
        end
    end

    local function draw_quest(x, y)
        local pulse = math.floor(gtime / 20) % 3 + 1
        screen.rect(x, y, TS, TS, P.qBase[pulse])
        screen.rect(x, y, TS, 1, PAL.qLight)
        screen.rect(x, y + TS - 1, TS, 1, PAL.qDark)
        -- ? 字形
        screen.rect(x + 3, y + 2, 5, 2, 0xFCFCFC)
        screen.rect(x + 6, y + 4, 2, 2, 0xFCFCFC)
        screen.rect(x + 4, y + 6, 2, 2, 0xFCFCFC)
        screen.rect(x + 4, y + 9, 2, 2, 0xFCFCFC)
    end

    local function draw_block_run(t, s, e, y)
        local x0 = s * TS - camx
        local x1 = (e + 1) * TS - camx
        local w = x1 - x0
        if t == "H" then
            screen.rect(x0, y, w, TS, PAL.hard)
            screen.rect(x0, y, w, 1, PAL.hardL)
            screen.rect(x0, y + TS - 1, w, 1, PAL.hardD)
            for tc = s + 1, e do
                D(tc * TS - camx, y + 1, 1, TS - 2, PAL.hardD)
            end
        elseif t == "b" then      -- 敲空的砖
            screen.rect(x0, y, w, TS, P.gBase)
            screen.rect(x0, y + TS - 1, w, 1, P.gMortar)
            for tc = s, e do
                D(tc * TS - camx + 4, y + 4, 4, 4, P.gMortar)
            end
        else                      -- B 砖：交错砖缝
            screen.rect(x0, y, w, TS, P.brick)
            screen.rect(x0, y + TS - 1, w, 1, P.mortar)
            for tc = s, e do
                D(tc * TS - camx + 5, y, 1, 5, P.mortar)
                D(tc * TS - camx + TS - 1, y + 6, 1, 5, P.mortar)
            end
        end
    end

    local function draw_pipe(s, e, r, gy)
        local x = s * TS - camx
        local w = (e - s + 1) * TS
        local top = r * TS + HUD_H
        screen.rect(x - 2, top, w + 4, 5, PAL.pipe)             -- 喇叭口
        screen.rect(x - 2, top, w + 4, 2, PAL.pipeL)
        screen.rect(x, top + 5, w, gy - top - 5, PAL.pipe)      -- 管身
        screen.rect(x + 2, top + 5, 3, gy - top - 5, PAL.pipeL) -- 左高光
        screen.rect(x + w - 5, top + 5, 3, gy - top - 5, PAL.pipeD) -- 右暗部
    end

    local function draw_flag(gy)
        local x = flag_c * TS - camx + 5
        local top = 6 * TS + HUD_H
        screen.rect(x, top, 2, gy - top, 0xC8C8C8)     -- 杆
        screen.rect(x - 1, top - 3, 4, 3, 0xFCFCFC)    -- 顶球
        local slide = 0
        if state == "clear" then
            slide = math.min(state_t * 1.5, gy - top - 20)
        end
        local fy = top + 4 + slide
        screen.rect(x - 7, fy, 7, 6, PAL.flagG)        -- 旗
        screen.rect(x - 6, fy + 6, 5, 3, PAL.flagD)
        screen.rect(flag_c * TS - camx, gy - TS, TS, TS, PAL.hard)  -- 底座
    end

    local function draw_tiles(gy, c0, c1)
        for r = 0, GROUND_ROW - 1 do
            local y = r * TS + HUD_H
            local c = c0
            while c <= c1 do
                local t = tile_at(c, r)
                if t == "B" or t == "b" or t == "H" then
                    local s = c
                    while c <= c1 and tile_at(c, r) == t do c = c + 1 end
                    draw_block_run(t, s, c - 1, y)
                elseif is_quest(t) then
                    draw_quest(c * TS - camx, y)
                    c = c + 1
                elseif t == "C" then
                    local ph = math.floor(gtime / 6) % 4
                    local cw = ({8, 5, 2, 5})[ph + 1]
                    local cx = c * TS - camx + math.floor((TS - cw) / 2)
                    screen.rect(cx, y + 2, cw, 8, PAL.coin)
                    if cw >= 5 then
                        screen.rect(cx + 1, y + 3, cw - 2, 6, PAL.coinL)
                    end
                    c = c + 1
                elseif t == "P" then
                    local s = c
                    while c <= c1 and tile_at(c, r) == "P" do c = c + 1 end
                    draw_pipe(s, c - 1, r, gy)
                else
                    c = c + 1   -- p/F 由管道/旗杆整件绘制
                end
            end
        end
        if flag_c and flag_c >= c0 - 1 and flag_c <= c1 + 1 then
            draw_flag(gy)
        end
    end

    -- ---------------------------------------------------------- 主绘制
    local function draw()
        rc = 0
        decor = 0
        P = lv and lv.underground and UG or PAL
        camx = math.floor(cam_x or 0)

        if not lv or state == "win" then
            screen.begin(0x000000)
            screen.rect(58, 82, 124, 74, 0x0C1622)
            screen.rect(58, 82, 124, 2, 0x3E5A7C)
            screen.rect(58, 154, 124, 2, 0x3E5A7C)
            screen.text("通关！", 96, 92, PAL.coin)
            screen.text("得分 " .. score, 88, 116, 0xFFFFFF)
            if gtime % 48 < 30 then screen.text("A键重来", 92, 138, PAL.coin) end
            screen.present()
            return
        end

        if state == "ready" then
            -- SMB 式黑屏报幕
            screen.begin(0x000000)
            draw_hud()
            local title = lv.underground and "地下金币关" or ("第 " .. lv_idx .. " 关")
            screen.text(title, lv.underground and 80 or 96, 78, 0xFFFFFF)
            draw_mario(100, 118, player.big, 1, 0, 0)
            screen.rect(96, 130, 24, 2, PAL.gTop)
            screen.rect(96, 132, 24, 5, PAL.gBase)
            screen.text("× " .. lives, 122, 124, 0xFFFFFF)
            if gtime % 48 < 30 then screen.text("A键开始", 92, 168, PAL.coin) end
            screen.present()
            return
        end

        screen.begin(lv.underground and UG.sky or PAL.sky)
        draw_hud()

        local gy = GROUND_ROW * TS + HUD_H     -- 地表 y = 200
        local c0 = math.floor(camx / TS)
        local c1 = c0 + COLS + 1

        draw_scenery(gy)
        draw_ground(gy, c0, c1)
        draw_tiles(gy, c0, c1)

        -- 道具
        if items_anim then
            for _, it in ipairs(items_anim) do
                local x = math.floor(it.x - camx)
                if x >= -14 and x < W + 14 and it.y >= 0 and it.y < H + TS then
                    draw_item(it, x)
                end
            end
        end

        -- 敌人
        for _, e in ipairs(enemies) do
            if not e.dead and e.y < H + 12 and e.x > camx - 14 and e.x < camx + W + 14 then
                local x = math.floor(e.x - camx)
                if e.kind == "goomba" then
                    draw_goomba(x, math.floor(e.y), e.squash)
                else
                    draw_koopa(x, math.floor(e.y),
                               e.vx + (e.shell and e.shell_vx or 0) >= 0 and 1 or -1,
                               e.shell, e.shell and e.shell_vx ~= 0)
                end
            end
        end

        -- 玩家
        local p = player
        local ph = p.big and 16 or 12
        local py = math.floor(p.y) - ph
        local px = math.floor(p.x - 6 - camx)
        if py < H - 2 then
            local pose
            if not p.on_ground and state == "play" then
                pose = 3
            elseif math.abs(p.vx) > 0.2 then
                pose = math.floor(p.anim / 4) % 2 + 1
            else
                pose = 0
            end
            draw_mario(px, py, p.big, p.facing, p.star, pose)
        end

        -- 状态面板
        if state == "clear" and state_t > 50 then
            screen.rect(58, 86, 124, 52, 0x0C1622)
            screen.rect(58, 86, 124, 2, 0x3E5A7C)
            screen.rect(58, 136, 124, 2, 0x3E5A7C)
            screen.text("过关！", 96, 94, PAL.coin)
            screen.text("+" .. math.max(0, math.floor(timer_val)) * 10 .. " 分", 86, 116, 0xFFFFFF)
        elseif state == "over" then
            screen.rect(58, 82, 124, 74, 0x0C1622)
            screen.rect(58, 82, 124, 2, 0x3E5A7C)
            screen.rect(58, 154, 124, 2, 0x3E5A7C)
            screen.text("游戏结束", 88, 92, 0xFFFFFF)
            screen.text("得分 " .. score, 88, 116, 0xFFFFFF)
            if gtime % 48 < 30 then screen.text("A键重来", 92, 138, PAL.coin) end
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
        elseif state == "over" or state == "win" then
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
                ui.beep(500, 20)
            end
        elseif id == "function" then
            -- 无手柄时单击=左移一步
            key_left = 6
        elseif id == "down" then
            -- 管道入口（站管顶按下）：检查脚下
            local pc = math.floor(player.x / TS)
            local pr = math.floor((player.y - HUD_H) / TS)
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
                end
            end
        end
    end

    -- ---------------------------------------------------------- 状态机
    local function state_step()
        if state == "die" then
            if state_t == 0 then player.vy = -4 end   -- 经典死亡上抛
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
        color = PAL.mRed,
        enter = function(self)
            if not player then reset_game() end
            refresh()
        end,
        tick = function(self, dt)
            gtime = gtime + 1
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


-- ---- 独立应用全局回调 ----
function on_start()
    if _app then _app:enter(_app) end
end

function on_tick(dt)
    if _app then
        _app:tick(_app, dt)
        _app:draw(_app)
    end
end

function on_button_click(id)
    if _app and _app.key then _app:key(id) end
end

function on_exit()
    if _app and _app.leave then _app:leave(_app) end
end
