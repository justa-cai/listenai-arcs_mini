-- 蜂鸣器音乐：用设备蜂鸣器（方波单音）演奏几首经典曲目。
--
-- 曲目与音符序列来自 ./tmp/… 的主机侧调研（deepseek_web_search）：先筛"旋律线条
-- 清晰、音域窄、节奏规整"的曲子 —— 蜂鸣器只有单音没有和声，旋律本身得够抓耳，
-- 大脑才会自动补上缺失的伴奏。全部曲子的音域落在 G3(196Hz)~C6(1047Hz)，也就是
-- 设备 buzzer 接口允许的 100..5000Hz 中段。
--
-- 播放走固件的整曲接口 buzzer.play_seq(text)：固件边合成边把 PCM 流式推给播放器，
-- 音符之间**没有缝**。这一条是必须的 —— 早先用"每个音一个 buzzer.play"，固件对
-- 每个音都要 stop/重建 WAV/再 play（实测每次约 40ms），加上任务 20ms 一次的轮询，
-- 每个音都带 ~50ms 死区，而且是逐音累加的：八分音符 250ms 被拉成 300ms、四分
-- 500ms 变成 550ms，时值比例被改掉，听感就是"BPM 对了但节奏不对"。
--
-- play_seq 的文本是 "freq:ms,freq:ms,..."，freq = 0 表示休止，空串表示停止。
-- 每个音在**按下播放的瞬间**按当前变速算好时长，整串交给固件 —— 播完之前 Lua
-- 这边不再参与节拍，只负责进度条的显示。
--
-- 几个必须守住的点：
--   * 频率/时值越界会让固件 luaL_error 打死整个桌面 —— 曲谱里的每个音在打包前
--     都用 ./tmp/music_check.lua 校验过范围（freq 0 或 100..5000，ms 20..10000）。
--   * 休止就是 freq = 0，绝不能拼出 "0Hz 的音"。
--   * 蜂鸣开关（设置里的"按键音"）挡的是 UI 反馈音，不是音乐 —— 这里直接调
--     buzzer，不经过 ui.beep，否则静音键会把音乐也一起哑掉。
do
    local C = ui.C

    -- 十二平均律，C4=262Hz 起。R = 休止（不发声）。
    local N = {
        R = 0,
        G3 = 196, A3 = 220, B3 = 247,
        C4 = 262, D4 = 294, E4 = 330, F4 = 349, G4 = 392,
        ["G#4"] = 415, A4 = 440, ["A#4"] = 466, B4 = 494,
        C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784,
        A5 = 880, B5 = 988, C6 = 1047, D6 = 1175, E6 = 1319,
    }

    -- 曲谱记法：空格分隔的音符。每个音是 "音名" 或 "音名:拍数"，拍数的单位
    -- 由每首曲子自己的 unit 决定（毫秒）。用了 `*`/`:` 之外不加别的语法 ——
    -- 少一点解析分支就少一处能崩的地方。
    local SONGS = {
        { name = "小星星", bpm = 120, notes = [[
            C4:2 C4:2 G4:2 G4:2 A4:2 A4:2 G4:4 F4:2 F4:2 E4:2 E4:2 D4:2 D4:2 C4:4 G4:2 G4:2 F4:2
            F4:2 E4:2 E4:2 D4:4 G4:2 G4:2 F4:2 F4:2 E4:2 E4:2 D4:4 C4:2 C4:2 G4:2 G4:2 A4:2 A4:2
            G4:4 F4:2 F4:2 E4:2 E4:2 D4:2 D4:2 C4:4
        ]] },
        { name = "两只老虎", bpm = 120, notes = [[
            C4:2 D4:2 E4:2 C4:2 C4:2 D4:2 E4:2 C4:2 E4:2 F4:2 G4:4 E4:2 F4:2 G4:4 G4:2 A4:2 G4:2
            F4:2 E4:2 C4:2 G4:2 A4:2 G4:2 F4:2 E4:2 C4:2 D4:2 G4:2 C4:4 D4:2 G4:2 C4:4
        ]] },
        { name = "小羊羔", bpm = 120, notes = [[
            E4:2 D4:2 C4:2 D4:2 E4:2 E4:2 E4:4 D4:2 D4:2 D4:4 E4:2 G4:2 G4:4 E4:2 D4:2 C4:2 D4:2
            E4:2 E4:2 E4:2 E4:2 D4:2 D4:2 E4:2 D4:2 C4:8
        ]] },
        { name = "小蜜蜂", bpm = 120, notes = [[
            E4:2 C4:2 D4:2 E4:2 E4:2 D4:2 D4:4 D4:2 D4:2 E4:2 D4:2 C4:4 D4:2 D4:2 E4:2 D4:2 C4:2
            D4:2 E4:2 E4:2 E4:2 D4:2 D4:2 D4:2 C4:4
        ]] },
        { name = "欢乐颂", bpm = 120, notes = [[
            E4:2 E4:2 F4:2 G4:2 G4:2 F4:2 E4:2 D4:2 C4:2 C4:2 D4:2 E4:2 E4:3 D4:1 D4:4 E4:2 E4:2
            F4:2 G4:2 G4:2 F4:2 E4:2 D4:2 C4:2 C4:2 D4:2 E4:2 D4:3 C4:1 C4:4
        ]] },
        { name = "生日快乐", bpm = 100, notes = [[
            G4:1 G4:1 A4:2 G4:2 C5:2 B4:4 G4:1 G4:1 A4:2 G4:2 D5:2 C5:4 G4:1 G4:1 G5:2 E5:2 C5:2
            B4:4 F5:1 F5:1 E5:2 C5:2 D5:2 C5:4
        ]] },
        { name = "铃儿响叮当", bpm = 150, notes = [[
            E5:2 E5:2 E5:4 E5:2 E5:2 E5:4 E5:2 G5:2 C5:2 D5:2 E5:8 F5:2 F5:2 F5:2 F5:2 F5:2 E5:2
            E5:2 E5:2 E5:2 D5:2 D5:2 E5:2 D5:4 G5:4
        ]] },
        -- 超级马里奥地上关主题。用通行的 8 小节扒谱（含 A 段动机与 B 段续句），
        -- **完整走两遍** —— 单遍只有 6.9 秒、又收在导音 B4 上，听起来像"刚起头就
        -- 没了"，两遍 18 秒就和别的曲子一个量级了。
        --
        -- 时值以八分为单位：`:1`=八分、`:2`=四分、`:4`=二分。之前那版把四分音符
        -- 写成了八分（"节奏赶"的根源）—— 第 2~5 小节的 G5/G4/C5/E4/A4/B4 都是
        -- 四分音符，第 1 小节那串 E5 才是八分。
        { name = "超级马里奥", bpm = 200, notes = [[
            E5:1 E5:1 R:1 E5:1 R:1 C5:1 E5:1 R:1
            G5:2 R:2 G4:2 R:2
            C5:2 R:2 G4:2 R:2
            E4:2 R:2 A4:2 R:2
            B4:2 R:2 A#4:2 A4:2
            G4:1 E5:1 G5:1 A5:2 R:2 F5:1 G5:1
            R:2 E5:2 R:2 C5:2 D5:2 B4:2
            R:2
            E5:1 E5:1 R:1 E5:1 R:1 C5:1 E5:1 R:1
            G5:2 R:2 G4:2 R:2
            C5:2 R:2 G4:2 R:2
            E4:2 R:2 A4:2 R:2
            B4:2 R:2 A#4:2 A4:2
            G4:1 E5:1 G5:1 A5:2 R:2 F5:1 G5:1
            R:2 E5:2 R:2 C5:2 D5:2 B4:2
            R:2
        ]] },
        { name = "天空之城", bpm = 100, notes = [[
            A4:2 B4:2 C5:2 B4:2 C5:2 E5:2 B4:4 E5:2 A4:2 G4:2 A4:2 C5:2 G4:4 E5:1 E5:1 F4:2 C5:2
            B4:2 C5:2 D5:2 E5:2 C5:2 C5:4 C5:2 B4:2 A4:2 A4:2 B4:2 G#4:2 A4:4 C5:1 D5:1 E5:2
            D5:2 E5:2 G5:2 D5:4 G5:2 C5:2 B4:2 C5:2 E5:2 E5:4 R:2 A4:2 G4:2 E5:1 D5:1 C5:4 D5:2
            C5:2 D5:2 G5:2 E5:4 E5:2 C5:2 D5:2 B4:2 A4:2 B4:1 A4:1 C5:2 D5:2 E5:2 C5:2 C5:4 C5:2
            B4:2 A4:2 A4:2 B4:2 G#4:2 A4:4
        ]] },
    }

    -- 解析 + 预算每首的总时长；每个音的**绝对起点**一并算好。
    --
    -- 时值单位统一成**八分音符**：`:1`=八分、`:2`=四分、`:3`=附点四分、`:4`=二分、
    -- `:8`=全音符；`bpm` 是这首曲子的标准速度（四分音符每分钟），所以
    --     unit_ms = 60000 / bpm / 2
    -- 之前几首曲子各用各的单位（有的把一拍写成 `:1`、有的写成 `:2`），比例就错了
    -- 一倍 —— 生日快乐那句"Hap-py"尤其明显：它是**两个八分音符**的弱起，写成两个
    -- 四分音符整首就慢一半。所以这里统一单位，让每首的时值都从同一把尺子上量。
    --
    -- 音符起点与 tick（20ms）的量化误差最大 ±10ms（下面用四舍五入而不是向上取整）。
    -- 这是固件 tick 粒度决定的，改不掉；但偏差是**有界**的、不累积，听感上没有漂移。
    local function compile(song)
        local unit = 60000 / song.bpm / 2
        assert(unit >= 20, song.name .. ": bpm 太高")
        local seq, at = {}, 0
        for tok in song.notes:gmatch("%S+") do
            local name, mult = tok:match("^(%a[#%d]*):(%d+)$")
            if not name then name, mult = tok, 1 end
            local f = N[name]
            assert(f ~= nil, song.name .. " 有未知音符 " .. tok)
            local ms = math.floor(unit * tonumber(mult) + 0.5)
            seq[#seq + 1] = { name = name, f = f, ms = ms, at = at }
            at = at + ms
        end
        assert(#seq > 0, song.name .. " 没有音符")
        assert(#seq <= 192, song.name .. " 音符数超过固件上限 192")
        song.seq, song.total, song.unit = seq, at, unit
    end
    for i = 1, #SONGS do compile(SONGS[i]) end

    -- 固件要支持整曲才有得玩。老固件没有这个函数 —— 明说，不要静悄悄地什么都不响。
    local HAS_SEQ = ui.HAS_BUZZER and type(buzzer.play_seq) == "function"

    local idx = 1
    local playing, t, tempo = false, 0, 100   -- tempo 是速度百分比（100 = 曲谱标准 BPM）
    local total = 0                            -- 本首按当前变速算出来的实际总时长
    local note = 0                             -- "音符列车"高亮的那个音（由 t 反查）

    local function song() return SONGS[idx] end

    local function stop()
        if playing and HAS_SEQ then buzzer.play_seq("") end   -- 空串 = 停
        playing, t, note = false, 0, 0
    end

    -- 把当前曲子按 tempo 拼成固件要的 "freq:ms,..." 整串。
    -- 每次按播放现拼（73 个音，一次几微秒），免得缓存和变速/换曲不同步。
    local function build_text(sg)
        local scale = 100 / tempo
        local out = {}
        local sum = 0
        for i = 1, #sg.seq do
            local n = sg.seq[i]
            local ms = math.floor(n.ms * scale + 0.5)
            -- 固件对每个音的时值也做硬校验：20..10000ms（整曲没有 WAV 缓冲区限制，
            -- 所以上限比单音的 3000 宽得多，50% 变速下全音符 4s 也放得下）
            if ms < 20 then ms = 20 elseif ms > 10000 then ms = 10000 end
            out[#out + 1] = n.f .. ":" .. ms
            sum = sum + ms
        end
        return table.concat(out, ","), sum
    end

    -- 按当前曲子 + 变速算出总时长，只给进度条/时间显示用（换曲、调速时算一次，
    -- 不在每帧里算 —— 73 个音的取整循环放 draw 里是白烧指令预算）。
    local function refresh_total()
        local _, sum = build_text(song())
        total = sum
    end

    local function select(i)
        stop()
        idx = ((i - 1) % #SONGS) + 1
        refresh_total()
        ui.refresh()
    end

    local function set_tempo(v)
        if v < 50 then v = 50 elseif v > 150 then v = 150 end
        if v == tempo then return end
        tempo = v
        if playing then                       -- 变速要让固件按新时值重来一遍
            playing = false
            if HAS_SEQ then buzzer.play_seq("") end
        end
        refresh_total()
        ui.refresh()
    end

    local function toggle()
        if not HAS_SEQ then
            ui.notice("固件需更新才支持整曲播放")
            return
        end
        if playing then
            stop()
        else
            local text = build_text(song())
            buzzer.play_seq(text)
            playing, t, note = true, 0, 0
        end
        ui.refresh()
    end

    local function clock(ms)
        local s = math.floor(ms / 1000)
        return string.format("%d:%02d", math.floor(s / 60), s % 60)
    end

    local function draw()
        ui.begin("蜂鸣器音乐")
        local sg = song()

        -- 标题只放"曲号 + 曲名"（再加变速百分比，仅在偏离标准时显示），
        -- 整条必须塞进 240px，多了会被右边切掉。
        ui.text(tempo == 100 and string.format("%d/%d  %s", idx, #SONGS, sg.name)
                or string.format("%d/%d  %s  %d%%", idx, #SONGS, sg.name, tempo),
                12, 32, C.text)

        -- 音符列车：当前音 + 往后 11 个音，柱高按音高；休止符画成小点
        local slot, base = 18, 112
        for k = 0, 11 do
            local n = sg.seq[note + k]
            local x = 12 + k * slot
            local here = (k == 0 and playing)
            if n then
                if n.f > 0 then
                    local h = 6 + math.floor((n.f - 196) * 26 / (1047 - 196))
                    if h > 32 then h = 32 elseif h < 6 then h = 6 end
                    screen.rect(x, base - h, 11, h, here and C.hi or C.line)
                else
                    screen.rect(x, base - 3, 11, 3, here and C.warn or C.line)
                end
            else
                screen.rect(x, base - 2, 11, 2, C.tile_dim)
            end
        end

        -- 当前音名 + 频率（大字那行没有，就写清楚现在在响什么）
        if playing and sg.seq[note] then
            local n = sg.seq[note]
            ui.text(n.f > 0 and string.format("当前 %s   %d Hz", n.name, n.f)
                            or "当前 休止", 12, 124, C.hi)
        else
            ui.text(note == 0 and "按 A / 双击开始" or "播放完毕", 12, 124, C.dim)
        end

        -- 进度条。total 是"按当前变速算出来的实际时长"，在换曲/调速时更新过。
        local filled = total > 0 and math.floor(216 * t / total) or 0
        if filled > 216 then filled = 216 end
        screen.rect(12, 150, 216, 8, C.tile_dim)
        if filled > 0 then screen.rect(12, 150, filled, 8, C.hi) end
        ui.text(("%s / %s"):format(clock(t), clock(total)), 12, 164, C.dim)

        if not ui.HAS_BUZZER then
            ui.footer("本机无蜂鸣器接口")
        elseif not HAS_SEQ then
            ui.footer("固件需更新才支持整曲播放")
        else
            ui.footer(string.format("%d BPM · 左右换曲 · 上下调速", sg.bpm))
        end

        ui.commands({
            { label = "返回", run = function() stop(); ui.back() end },
            { label = playing and "停止" or "播放", run = toggle },
            { label = "上一首", run = function() select(idx - 1) end },
            { label = "下一首", run = function() select(idx + 1) end },
            { label = "慢一点", run = function() set_tempo(tempo - 10) end },
            { label = "快一点", run = function() set_tempo(tempo + 10) end },
        })
    end

    ui.register({
        id = "music",
        name = "蜂鸣器音乐",
        icon = "music",
        color = C.hi,
        enter = function(self) stop(); refresh_total(); ui.refresh() end,
        tick = function(self, dt)
            if not playing then return end
            -- 整曲的节拍在固件那边，这里只跟着走：推进进度条，并沿谱面的 at
            -- 前进"音符列车"的高亮位置。
            t = t + dt
            local sg = song()
            local scale = 100 / tempo
            while note < #sg.seq and t >= sg.seq[note + 1].at * scale do
                note = note + 1
            end
            if t >= total then
                playing = false
                t = total
            end
            ui.refresh()
        end,
        draw = function(self) draw() end,
        -- 方向键直接换曲/调速：不用先把命令焦点挪到"下一首"（手柄上尤其顺手）
        key = function(self, id)
            if id == "left" then select(idx - 1); return true end
            if id == "right" then select(idx + 1); return true end
            if id == "up" then set_tempo(tempo + 10); return true end
            if id == "down" then set_tempo(tempo - 10); return true end
            return false
        end,
        -- 离开就把整曲停掉（play_seq("") 能立刻停 —— 走的是固件的中止路径，
        -- 不是等最后一个音自己放完）。
        leave = function(self) stop() end,
    })
end
