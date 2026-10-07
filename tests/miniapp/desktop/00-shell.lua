-- ============================================================================
-- 桌面 shell —— 应用注册表 / 按键分发 / 绘制助手 / 存档中枢 / 主循环
--
-- 运行环境一次只加载一个 Lua 文件（无 load/dofile、无文件系统），所以"桌面 +
-- 全部应用 + 全部游戏"必须是同一个文件；源码放在 tests/miniapp/desktop/，
-- 由 ./lua.sh bundle 20-desktop 拼成 20-desktop.lua。本文件必须最先拼接：
-- 它声明 ui 表，后面的文件只负责 ui.register{...}。
--
-- 按键模型（长按 3 秒被系统拿去退出小应用，脚本收不到）：方向键 = 移动；
-- function_double（设备双击、手柄 A）= 激活；function（设备单击）= 下一个，
-- 无手柄时是唯一移动方式；back（手柄 叉）= 返回桌面，任何应用里都生效；
-- settings（手柄 SELECT）= 一步跳到"设置"。
--
-- 硬约束（docs/miniapp.md）：每帧 ≤128 矩形、≤8 段文字且每段 ≤63 字节（16px
-- 固定字体）；没有 pcall —— 任何 Lua 错误都会终止整个桌面实例，且若读过存档
-- 还会清空它。所以每个绘图/回调都要自己守住边界。
-- ============================================================================

assert(type(app) == "table" and app.api_version >= 4, "DESKTOP: API 4 required")
assert(screen and app.width and app.height, "DESKTOP: screen required")

local W, H = app.width, app.height
local HAS_LED = type(led) == "table"
local HAS_BUZZER = type(buzzer) == "table"
local HAS_CLOCK = type(clock) == "table"
local HAS_HTTP = type(http) == "table" and type(json) == "table"
local HAS_TTS = type(tts) == "table"

-- ---------------------------------------------------------------------- 调色板
local C = {
    bg       = 0x0D1418,
    bar      = 0x16232B,
    tile     = 0x223742,
    tile_dim = 0x18262E,
    hi       = 0x5FD08A,
    text     = 0xE8F1F5,
    dim      = 0x86A0AD,
    line     = 0x2A3A45,
    warn     = 0xFFB454,
    bad      = 0xFF6B6B,
}

-- ---------------------------------------------------------------------- ui 表
-- 后面的文件通过它取壳层能力（一个 upvalue，省局部变量额度）
local ui = {}
ui.W, ui.H, ui.C = W, H, C
ui.HAS_LED, ui.HAS_BUZZER, ui.HAS_CLOCK = HAS_LED, HAS_BUZZER, HAS_CLOCK
ui.HAS_HTTP, ui.HAS_TTS = HAS_HTTP, HAS_TTS

-- 文本 ≤63 字节，且不能把一个 UTF-8 字符切一半（切坏固件侧会报错）
local function clip(s, limit)
    s = tostring(s)
    if #s <= limit then return s end
    local cut = limit
    while cut > 0 do
        local b = string.byte(s, cut + 1)
        if b and b >= 0x80 and b < 0xC0 then cut = cut - 1 else break end
    end
    return string.sub(s, 1, cut)
end
ui.clip = clip

local function text(s, x, y, color)
    screen.text(clip(s, 63), x, y, color or C.text)
end
ui.text = text

-- 文本像素宽估算：ASCII 8px，其余（中文）16px
local function text_width(s)
    local w = 0
    for _, cp in utf8.codes(s) do
        w = w + (cp < 0x80 and 8 or 16)
    end
    return w
end

function ui.text_center(s, y, color)
    local x = math.floor((W - text_width(s)) / 2)
    if x < 0 then x = 0 end
    text(s, x, y, color)
end

-- 应用统一用它开屏（含标题栏），保证样式一致
function ui.begin(title)
    screen.begin(C.bg)
    screen.rect(0, 0, W, 26, C.bar)
    text(title, 10, 5, C.text)
end

-- 提示行：坐在命令条上方，应用内容不要侵占这一带
function ui.footer(s, color)
    text(s, 10, H - 52, color or C.dim)
end

-- ---------------------------------------------------------------------- 注册表
local apps = {}
ui.apps = apps

function ui.register(def)
    assert(type(def) == "table", "register: def must be a table")
    assert(type(def.id) == "string" and #def.id > 0, "register: id required")
    assert(type(def.name) == "string" and #def.name > 0, "register: name required")
    assert(type(def.tick) == "function", "register: tick required")
    for i = 1, #apps do
        assert(apps[i].id ~= def.id, "register: duplicate id " .. def.id)
    end
    apps[#apps + 1] = def
end

-- ---------------------------------------------------------------------- 存档中枢
-- 整机只有 4 个存档槽、每份 1 KiB、每槽 10 秒一次写，按安装包 id 隔离 —— 桌面和
-- 全部应用共用这一份，所以写入集中在这里：唯一 writer、限流、失败降级。
local store = { data = {}, dirty = false, last_ms = -100000, note = "" }

local SAVE_INTERVAL_MS = 10000
local RETRYABLE = {
    rate_limited = true, clock_unavailable = true, io_error = true,
    no_memory = true, unavailable = true,
}

local function store_init()
    if type(storage) ~= "table" then return end
    local d = storage.load()
    if type(d) == "table" then store.data = d end
end

function ui.load(key, default)
    local v = store.data[key]
    if v == nil then return default end
    return v
end

function ui.save(key, value)
    if store.data[key] == value then return end
    store.data[key] = value
    store.dirty = true
end

function ui.save_key_count()
    local n = 0
    for _ in pairs(store.data) do n = n + 1 end
    return n
end

function ui.save_clear()
    store.data = {}
    store.dirty = true
    store.note = ""
end

function ui.save_note() return store.note end

local function store_flush(now)
    if not store.dirty or type(storage) ~= "table" then return end
    if now - store.last_ms < SAVE_INTERVAL_MS then return end
    store.last_ms = now
    local ok, reason = storage.save(store.data)
    if ok == true then
        store.dirty, store.note = false, ""
    elseif RETRYABLE[reason] then
        -- 未校时 / 被限流 / 底层忙：保留脏标记，下一轮再试
        store.note = (reason == "clock_unavailable") and "未校时, 稍后再存" or "稍后重试保存"
    else
        -- storage_full / invalid_data / too_large：重试也不会成功
        store.dirty = false
        store.note = "存档失败: " .. tostring(reason)
    end
end

-- ---------------------------------------------------------------------- 输出外设
local led_mode = "off"
function ui.led(mode)
    if not HAS_LED or led_mode == mode then return end
    led_mode = mode
    if mode == "on" then led.on("status")
    elseif mode == "slow" then led.blink("status", 500, 500)
    elseif mode == "fast" then led.blink("status", 100, 900)
    else led.off("status") end
end

-- 蜂鸣是尽力而为（TTS/闹钟在播时会被丢弃），不阻塞也不报错；开关可在"设置"里改
local beep_on = true
function ui.beep(hz, ms_len)
    if HAS_BUZZER and beep_on then buzzer.play(hz or 880, ms_len or 60) end
end
function ui.beep_on() return beep_on end
function ui.set_beep(on)
    beep_on = on and true or false
    ui.save("ui.beep", beep_on)
    ui.refresh()
end

-- 桌面手势 / 停留进入：开关本身写进存档，下次启动恢复
function ui.dwell_on() return dwell_enter end
function ui.set_dwell(on)
    dwell_enter = on and true or false
    ui.save("ui.dwell", dwell_enter)
    dwell_ms = 0
    ui.refresh()
end

function ui.localtime()
    if not HAS_CLOCK then return nil end
    return clock.localtime()
end

-- ---------------------------------------------------------------------- 桌面状态
local COLS, ROWS = 3, 2
local PER_PAGE = COLS * ROWS
local MARGIN, GAP = 12, 10
local CELL_W = math.floor((W - MARGIN * 2 - GAP * (COLS - 1)) / COLS)
local CELL_H = 70
-- 网格顶边: 标题栏 26 到提示行 188 之间居中排 2 行 (2*70 + 10 = 150)，上下各留 6px
local TOP = 32

local mode = "desk"          -- "desk" | "app"
local focus = 1
local cur = nil
local dwell_enter = false    -- 设置里可切
local dwell_ms = 0
local ms = 0                 -- 单调毫秒（dt 恒 20，用计数而不是时钟）
local dirty = true
local transient, transient_left = "", 0
local pad_seen = false       -- 收到过手柄的方向键 = 用"按 A 进入"的提示语

function ui.refresh() dirty = true end

function ui.notice(s, ticks)
    transient = tostring(s)
    transient_left = ticks or 40      -- 默认约 800ms
    dirty = true
end

-- ---------------------------------------------------------------------- 命令条
-- 底部一条命令栏：换项/执行。用一条居中文显当前命令，省文字额度（每帧 8 段）。
local cmds, cmd_focus = nil, 1
function ui.commands(items)
    if type(items) ~= "table" or #items == 0 then
        cmds, cmd_focus = nil, 1
        return
    end
    cmds = items
    if cmd_focus > #items then cmd_focus = #items end
    if cmd_focus < 1 then cmd_focus = 1 end
end

function ui.commands_clear()
    cmds, cmd_focus = nil, 1
end

local function draw_commands()
    if transient == "" and not cmds then return end
    local y = H - 32
    screen.rect(0, y, W, 32, C.bar)
    if transient ~= "" then
        -- 应用内的提示占用这条：否则提示画在标题栏会被应用自己的画盖掉
        ui.text_center(transient, y + 8, C.warn)
        return
    end
    screen.rect(10, y + 12, 8, 8, C.dim)          -- 左箭头
    screen.rect(W - 18, y + 12, 8, 8, C.dim)      -- 右箭头
    ui.text_center(cmds[cmd_focus].label, y + 8, C.hi)
end

function ui.back()
    if cur and cur.leave then cur.leave(cur) end
    cur = nil
    cmds, cmd_focus = nil, 1
    mode = "desk"
    dwell_ms = 0
    dirty = true
end

function ui.page_of(i) return math.floor((i - 1) / PER_PAGE) + 1 end
function ui.page_count() return math.max(1, math.ceil(#apps / PER_PAGE)) end

-- 手柄 (src/middleware/gamepad) 的按键 id：方向键=移动，A/START=function_double，
-- 叉/B=back，SELECT=settings。设备功能键仍是 function/function_double，两条路径共用。
local PAD_STEP = COLS        -- 上下键在扁平列表里跨一行

local function focus_by(delta)
    local n = #apps
    if n == 0 then return end
    focus = ((focus - 1 + delta) % n + n) % n + 1
    dwell_ms = 0
    dirty = true
end

-- 按 id 定位应用 (全局快捷键"直进设置"用)
local function focus_app_id(id)
    for i = 1, #apps do
        if apps[i].id == id then
            focus = i
            dwell_ms = 0
            dirty = true
            return true
        end
    end
    return false
end

-- 单调毫秒（dt 恒 20）；给需要"变化值"的应用做随机种子/计时用
function ui.ms() return ms end

local function enter(i)
    focus = i
    cur = apps[i]
    mode = "app"
    dwell_ms = 0
    -- 命令条默认停在第二项：每张命令表的第一项都是"返回"，若默认停在那里，
    -- 按 A 进应用后再按一次 A 就立刻退出去了。停到第一项之外 = "确认"落在真正的
    -- 动作上（开始/重开/清空…），想返回按一次左键或手柄的 叉。
    cmds, cmd_focus = nil, 2
    if cur.enter then cur.enter(cur) end
    dirty = true
end

-- ---------------------------------------------------------------------- 应用图标
-- 运行环境没有图片 API，所以图标全部用矩形拼。统一画在 ICON×ICON 的格子里，
-- 参数 (x, y, bg)：格子左上角 + 格子底色（画"圆环"要拿底色把中间镂空回去）。
-- 每个图标 ≤ 8 个矩形：桌面一屏 6 个格子，最坏 48 + 格子/标题 7 ≈ 55，远低于
-- 每帧 128 的上限。图标不占文字额度（每帧只有 8 段，桌面已经用掉 7 段）。
local ICON = 34

-- 实心八边形（用两个正交矩形并出来，近似圆）
local function oct(x, y, s, c)
    local m = math.floor(s / 4)
    local b = s - 2 * m
    screen.rect(x + m, y, b, s, c)
    screen.rect(x, y + m, s, b, c)
end

-- 圆环：外八边形 + 内八边形用底色填回
local function ring(x, y, s, t, bg, c)
    oct(x, y, s, c)
    local i = s - 2 * t
    local m = math.floor(i / 4)
    screen.rect(x + t + m, y + t, i - 2 * m, i, bg)
    screen.rect(x + t, y + t + m, i, i - 2 * m, bg)
end

local ICONS = {}

ICONS.clock = function(x, y, bg)
    ring(x, y, 34, 4, bg, C.hi)
    screen.rect(x + 16, y + 8, 3, 10, C.hi)     -- 分针: 12 点方向
    screen.rect(x + 18, y + 16, 6, 3, C.hi)     -- 时针: 3 点方向
end

ICONS.timer = function(x, y)
    local c = C.warn
    screen.rect(x + 3, y + 3, 28, 3, c)         -- 上下盖
    screen.rect(x + 3, y + 28, 28, 3, c)
    screen.rect(x + 8, y + 6, 18, 4, c)         -- 上锥（逐级收窄）
    screen.rect(x + 13, y + 10, 8, 4, c)
    screen.rect(x + 15, y + 14, 4, 6, c)        -- 腰部
    screen.rect(x + 13, y + 20, 8, 4, c)        -- 下锥
    screen.rect(x + 8, y + 24, 18, 4, c)
end

ICONS.stopwatch = function(x, y, bg)
    ring(x + 3, y + 6, 28, 4, bg, C.hi)
    screen.rect(x + 14, y + 1, 6, 7, C.hi)      -- 顶部按钮
    screen.rect(x + 16, y + 12, 3, 9, C.hi)     -- 指针
end

ICONS.led = function(x, y)
    local c = C.warn
    oct(x + 6, y + 1, 22, c)                    -- 灯泡
    screen.rect(x + 13, y + 22, 8, 4, c)        -- 颈部
    screen.rect(x + 12, y + 27, 10, 3, c)       -- 螺口
    screen.rect(x + 14, y + 31, 6, 2, c)
end

ICONS.todo = function(x, y)
    local c = C.hi
    for i = 0, 2 do
        screen.rect(x + 4, y + 5 + i * 10, 6, 6, c)      -- 勾选格
        screen.rect(x + 14, y + 7 + i * 10, 16, 3, c)    -- 待办行
    end
end

ICONS.calc = function(x, y, bg)
    screen.rect(x + 5, y + 2, 24, 30, C.warn)   -- 机身
    screen.rect(x + 8, y + 5, 18, 6, bg)        -- 显示屏
    for r = 0, 1 do
        for k = 0, 2 do
            screen.rect(x + 8 + k * 6, y + 15 + r * 7, 4, 4, bg)
        end
    end
end

ICONS.settings = function(x, y)
    screen.rect(x + 2, y + 6, 30, 3, C.dim)     -- 三条滑轨
    screen.rect(x + 2, y + 15, 30, 3, C.dim)
    screen.rect(x + 2, y + 24, 30, 3, C.dim)
    screen.rect(x + 20, y + 3, 7, 9, C.hi)      -- 各自不同位置的滑块
    screen.rect(x + 7, y + 12, 7, 9, C.hi)
    screen.rect(x + 15, y + 21, 7, 9, C.hi)
end

ICONS.about = function(x, y, bg)
    ring(x, y, 34, 4, bg, C.hi)
    screen.rect(x + 15, y + 8, 4, 4, C.hi)      -- i 的点
    screen.rect(x + 15, y + 15, 4, 11, C.hi)    -- i 的竖
end

ICONS.quote = function(x, y)
    local c = C.hi
    screen.rect(x + 5, y + 8, 9, 11, c)         -- 两个引号
    screen.rect(x + 5, y + 19, 5, 7, c)
    screen.rect(x + 19, y + 8, 9, 11, c)
    screen.rect(x + 19, y + 19, 5, 7, c)
end

ICONS.speak = function(x, y)
    screen.rect(x + 3, y + 13, 6, 8, C.hi)      -- 箱体
    screen.rect(x + 9, y + 9, 5, 16, C.hi)      -- 号角
    screen.rect(x + 14, y + 5, 5, 24, C.hi)
    screen.rect(x + 22, y + 12, 3, 10, C.warn)  -- 两道声波
    screen.rect(x + 27, y + 7, 3, 20, C.warn)
end

ICONS.music = function(x, y, bg)
    -- 双八分音符：两根符干 + 顶上横梁 + 两个符头，一眼看出来是"曲子"
    screen.rect(x + 8, y + 5, 3, 20, C.hi)      -- 左符干
    screen.rect(x + 23, y + 5, 3, 20, C.hi)     -- 右符干
    screen.rect(x + 11, y + 5, 15, 5, C.hi)     -- 横梁
    screen.rect(x + 2, y + 21, 10, 8, C.warn)   -- 左符头
    screen.rect(x + 17, y + 21, 10, 8, C.warn)  -- 右符头
    screen.rect(x + 12, y + 29, 4, 3, bg)       -- 让两个符头别连成一块
end

ICONS.snake = function(x, y, bg)
    screen.rect(x + 3, y + 19, 9, 9, C.hi)      -- 身体（折线）
    screen.rect(x + 12, y + 19, 9, 9, C.hi)
    screen.rect(x + 12, y + 10, 9, 9, C.hi)
    screen.rect(x + 21, y + 10, 9, 9, C.hi)     -- 头
    screen.rect(x + 25, y + 13, 3, 3, bg)       -- 眼
    screen.rect(x + 22, y + 26, 8, 8, C.warn)   -- 食物
end

ICONS.flappy = function(x, y)
    screen.rect(x + 23, y + 2, 9, 11, C.hi)     -- 上下管道
    screen.rect(x + 23, y + 21, 9, 11, C.hi)
    screen.rect(x + 5, y + 13, 13, 9, C.warn)   -- 鸟身
    screen.rect(x + 18, y + 16, 5, 4, C.warn)   -- 嘴
    screen.rect(x + 9, y + 16, 3, 3, C.bg)      -- 眼
end

ICONS.brick = function(x, y)
    local c = C.warn
    screen.rect(x + 2, y + 9, 13, 6, c)         -- 三行错缝砖
    screen.rect(x + 17, y + 9, 15, 6, c)
    screen.rect(x + 2, y + 17, 8, 6, c)
    screen.rect(x + 12, y + 17, 20, 6, c)
    screen.rect(x + 2, y + 25, 15, 6, c)
    screen.rect(x + 19, y + 25, 13, 6, c)
    screen.rect(x + 26, y + 1, 6, 6, C.hi)      -- 球
end

ICONS.g2048 = function(x, y)
    screen.rect(x + 3, y + 3, 13, 13, C.hi)     -- 四个大小不一的方块
    screen.rect(x + 19, y + 3, 12, 12, C.dim)
    screen.rect(x + 3, y + 19, 12, 12, C.dim)
    screen.rect(x + 19, y + 19, 12, 12, C.warn)
end

ICONS.sokoban = function(x, y, bg)
    screen.rect(x + 2, y + 10, 18, 18, C.warn)  -- 箱子
    screen.rect(x + 8, y + 16, 6, 6, bg)
    screen.rect(x + 22, y + 2, 10, 10, C.dim)   -- 目标点
    screen.rect(x + 25, y + 5, 4, 4, bg)
    screen.rect(x + 22, y + 22, 10, 10, C.text) -- 玩家
end

-- 格子内居中一行文字（ui.text_center 是相对整屏居中的）
local function cell_text(s, x, y, color)
    s = clip(s, 21)
    local tx = x + math.floor((CELL_W - text_width(s)) / 2)
    if tx < x then tx = x end
    text(s, tx, y, color)
end

-- ---------------------------------------------------------------------- 桌面绘制
local function draw_desk()
    screen.begin(C.bg)
    screen.rect(0, 0, W, 26, C.bar)

    local a = apps[focus]
    if transient ~= "" then
        text(transient, 10, 5, C.warn)
    elseif a then
        -- 标题只放"应用名 + 页码": 提示语移到下面那行，否则整条会超出 240px 被切掉
        text(a.name .. "  " .. ui.page_of(focus) .. "/" .. ui.page_count(), 10, 5, C.text)
    else
        text("桌面为空", 10, 5, C.dim)
    end

    local first = (ui.page_of(focus) - 1) * PER_PAGE
    for i = 0, PER_PAGE - 1 do
        local def = apps[first + i + 1]
        local c, r = i % COLS, math.floor(i / COLS)
        local x = MARGIN + c * (CELL_W + GAP)
        local y = TOP + r * (CELL_H + GAP)
        if def then
            local on = (first + i + 1 == focus)
            local bg = on and C.tile or C.tile_dim
            if on then screen.rect(x - 2, y - 2, CELL_W + 4, CELL_H + 4, C.hi) end
            screen.rect(x, y, CELL_W, CELL_H, bg)
            local icon = ICONS[def.icon]
            if icon then icon(x + math.floor((CELL_W - ICON) / 2), y + 7, bg) end
            cell_text(def.name, x, y + 47, on and C.hi or C.text)
        else
            screen.rect(x, y, CELL_W, CELL_H, C.tile_dim)
            screen.rect(x + 28, y + 31, 7, 7, C.line)      -- 空位: 一个虚点
        end
    end

    -- 底部提示行: 存档提示优先，其次是操作提示。措辞按"见过手柄没有"切换 ——
    -- 方向键两种输入都有，但"进入"在设备功能键上是双击、在手柄上是 A。
    if transient == "" then
        local note = store.note
        if note ~= "" then
            ui.footer(note, C.warn)
        elseif dwell_enter then
            ui.footer("方向键移动 停留自动进入")
        elseif pad_seen then
            ui.footer("方向键移动 A键进入")
        else
            ui.footer("方向键移动 双击进入")
        end
    end
    screen.present()
end

local function render()
    if mode == "app" and cur then
        if cur.draw then cur.draw(cur) end
        draw_commands()
        screen.present()
    else
        draw_desk()
    end
end

-- ---------------------------------------------------------------------- 生命周期
function on_start()
    store_init()
    dwell_enter = ui.load("ui.dwell", false) == true
    beep_on = ui.load("ui.beep", true) == true
    ui.led("off")
    -- 图标名写错的话桌面上只是少一张图，肉眼很难发现；这里一次性钉死
    for i = 1, #apps do
        assert(ICONS[apps[i].icon], "app " .. apps[i].id .. " has no icon")
    end
    render()                       -- 启动阶段必须至少提交一帧
end

function on_tick(dt)
    ms = ms + dt
    store_flush(ms)

    if transient_left > 0 then
        transient_left = transient_left - 1
        if transient_left == 0 then
            transient = ""
            dirty = true
        end
    end

    if mode == "desk" then
        if dwell_enter and #apps > 0 then
            dwell_ms = dwell_ms + dt
            if dwell_ms >= 1500 then enter(focus) end
        end
    elseif cur and cur.tick then
        cur.tick(cur, dt)
    end

    if dirty then
        dirty = false
        render()
    end
end

function on_button_click(button_id)
    -- 方向键只有手柄会发，见到就说明手柄在线: 提示语据此换成"A键进入"
    if not pad_seen and (button_id == "up" or button_id == "down" or
                         button_id == "left" or button_id == "right") then
        pad_seen = true
        dirty = true
    end

    if mode == "desk" then
        if button_id == "function_double" then
            if #apps > 0 then enter(focus) end
        elseif button_id == "down" then
            focus_by(PAD_STEP)
        elseif button_id == "up" then
            focus_by(-PAD_STEP)
        elseif button_id == "left" then
            focus_by(-1)
        elseif button_id == "right" then
            focus_by(1)
        elseif button_id == "settings" then
            if focus_app_id("settings") then enter(focus) end
        elseif button_id == "back" then
            ui.notice("已在桌面")
        else
            focus_by(1)          -- function: 设备功能键单击 = 下一个
        end
        return
    end

    -- 应用内：back 永远等于"返回桌面"，由壳层保留 —— raw 游戏游玩中会清掉
    -- 命令条，没有这条兜底用户会被困在应用里出不来。
    if button_id == "back" then
        ui.back()
        return
    end

    -- SELECT = 直进设置（从任何应用一步跳到设置，不用先返回）
    if button_id == "settings" and cur and cur.id ~= "settings" then
        if focus_app_id("settings") then
            if cur and cur.leave then cur.leave(cur) end
            cmds, cmd_focus = nil, 1
            mode = "desk"
            enter(focus)
        end
        return
    end

    -- 有命令条：方向键换焦点、A (function_double) 执行；设备单击也换焦点。
    -- 没有命令条时，声明 raw 的应用（游戏游玩中）自己收全部按键。
    if cmds then
        -- 应用可选实现 key(id)：返回 true = 这个键它自己用了（比如方向键滚动/走棋）。
        -- 这样"命令条的可发现性"和"方向键直接操作"能共存 —— 有手柄时不用先
        -- 把焦点移到命令上，没手柄时命令条又兜住了全部操作。
        if cur and cur.key and cur.key(cur, button_id) then
            dirty = true
            return
        end
        if button_id == "function_double" then
            local c = cmds[cmd_focus]
            if c and c.run then c.run() end
        elseif button_id == "left" or button_id == "up" then
            cmd_focus = (cmd_focus - 2) % #cmds + 1
            dirty = true
        elseif button_id == "function" or button_id == "right" or button_id == "down" then
            cmd_focus = cmd_focus % #cmds + 1
            dirty = true
        end
        return
    end
    if cur.click then cur.click(cur, button_id) end
end

function on_exit()
    ui.led("off")
    if mode == "app" and cur and cur.leave then cur.leave(cur) end
end

-- 异步结果转给当前应用（应用自己按请求 id 过滤；旧实例的结果不会到这里）
function on_http_response(id, response)
    if mode == "app" and cur and cur.http then cur.http(cur, id, response) end
end

function on_tts_result(id, result)
    if mode == "app" and cur and cur.tts then cur.tts(cur, id, result) end
end
