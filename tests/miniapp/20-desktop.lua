-- 由 ./lua.sh bundle 20-desktop 生成, 请勿手改 (源: tests/miniapp/desktop)
-- 产物行号 - 2 = 拼接后的源行号; 注释已在打包时清空
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
-- 时钟：真实日历时间（七段码画法取自 tests/miniapp/02-clock.lua）
-- 未校时时显示等待，不把 tick 累加值当作时间。
-- 结构约定：tick 只判断"要不要重画"，绘制统一放在 draw 里，壳层负责 present
-- 和底部的命令条（所以内容不要画到 y > H-40 的区域）。
do
    local C = ui.C
    local W, H = ui.W, ui.H

    local SEG = { "abcdef", "bc", "abdeg", "abcdg", "bcfg",
                  "acdfg", "acdefg", "abc", "abcdefg", "abcdfg" }
    local SHAPE = { a = {1, 0, 3, 1}, b = {4, 1, 1, 3}, c = {4, 5, 1, 3},
                    d = {1, 8, 3, 1}, e = {0, 5, 1, 3}, f = {0, 1, 1, 3}, g = {1, 4, 3, 1} }

    local function digit(value, x, y, s)
        for name in SEG[value + 1]:gmatch(".") do
            local r = SHAPE[name]
            screen.rect(x + r[1] * s, y + r[2] * s, r[3] * s, r[4] * s, C.hi)
        end
    end

    local last_key = ""

    local function current_key()
        local t = ui.localtime()
        if not t then return "wait" end
        return string.format("%d-%d-%d %d:%d:%d", t.year, t.month, t.day, t.hour, t.min, t.sec)
    end

    local function draw()
        local t = ui.localtime()
        ui.begin("时钟")

        if not t then
            ui.text_center("等待设备校时", 108, C.warn)
        else
            local s = math.max(1, math.floor((W - 32) / 40))
            local x = math.floor((W - 40 * s) / 2)
            local y = 54
            local ds = string.format("%02d%02d%02d", t.hour, t.min, t.sec)
            for i = 1, 6 do
                digit(tonumber(string.sub(ds, i, i)), x, y, s)
                x = x + 6 * s
                if i == 2 or i == 4 then
                    screen.rect(x, y + 2 * s, s, s, C.text)
                    screen.rect(x, y + 6 * s, s, s, C.text)
                    x = x + 2 * s
                end
            end

            ui.text_center(string.format("%04d-%02d-%02d", t.year, t.month, t.day), 140, C.text)
            local off = t.utc_offset or 0
            ui.text_center(string.format("UTC%s%02d:%02d", off < 0 and "-" or "+",
                                         math.floor(math.abs(off) / 3600),
                                         math.floor(math.abs(off) / 60) % 60), 166, C.dim)
        end

        ui.commands({ {label = "返回", run = ui.back} })
    end

    ui.register({
        id = "clock",
        name = "时钟",
        icon = "clock",
        color = C.hi,
        enter = function(self)
            last_key = ""
            ui.refresh()
        end,
        tick = function(self, dt)
            local k = current_key()
            if k ~= last_key then
                last_key = k
                ui.refresh()
            end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
-- 倒计时：只在应用打开期间有效（设备没有给小应用的闹钟 API），到点用蜂鸣提醒。
-- 计时用 tick 计数，不依赖系统时钟。
do
    local C = ui.C
    local minutes, remain_ms, running = 1, 60 * 1000, false
    local fired_note, beep_left = "", 0

    local function fmt(ms)
        if ms < 0 then ms = 0 end
        local s = math.ceil(ms / 1000)
        return string.format("%02d:%02d", math.floor(s / 60), s % 60)
    end

    local function draw()
        ui.begin("倒计时")
        ui.text_center(fmt(remain_ms), 86, remain_ms <= 0 and C.bad or C.hi)
        ui.text_center(running and "计时中" or (remain_ms <= 0 and "时间到" or "已暂停"), 128, C.dim)
        if fired_note ~= "" then ui.footer(fired_note, C.warn) end

        ui.commands({
            { label = "返回", run = ui.back },
            { label = running and "暂停" or "开始", run = function()
                  if remain_ms <= 0 then remain_ms = minutes * 60000 end
                  running = not running
                  fired_note = ""
                  ui.refresh()
              end },
            { label = "加 1 分钟", run = function()
                  minutes = minutes % 60 + 1
                  remain_ms = minutes * 60000
                  running = false
                  fired_note = ""
                  ui.refresh()
              end },
            { label = "归零", run = function()
                  running = false
                  remain_ms = minutes * 60000
                  fired_note = ""
                  ui.refresh()
              end },
        })
    end

    ui.register({
        id = "timer",
        name = "倒计时",
        icon = "timer",
        color = C.warn,
        enter = function(self)
            remain_ms = minutes * 60000
            running = false
            fired_note = ""
            beep_left = 0
            ui.refresh()
        end,
        tick = function(self, dt)
            if beep_left > 0 then
                -- 每 4 个 tick（约 80ms）响一次，共 4 次
                if beep_left % 4 == 0 then ui.beep(1500, 60) end
                beep_left = beep_left - 1
            end
            if not running then return end
            remain_ms = remain_ms - dt
            if remain_ms <= 0 then
                running = false
                remain_ms = 0
                fired_note = "时间到"
                beep_left = 16
                ui.led("fast")
                ui.refresh()
            else
                ui.refresh()                       -- 秒在跳，每 tick 重画
            end
        end,
        draw = function(self) draw() end,
        leave = function(self)
            running = false
            ui.led("off")
        end,
    })
end
-- 秒表：按 tick 计数计时（dt 恒 20ms），不依赖系统时钟，未校时也能用。
do
    local C = ui.C
    local elapsed, running, last_cs = 0, false, -1

    local function label()
        local cs = math.floor(elapsed / 100) % 10
        local total_s = math.floor(elapsed / 1000)
        return string.format("%02d:%02d.%d", math.floor(total_s / 60) % 100, total_s % 60, cs)
    end

    local function toggle()
        running = not running
        ui.beep(running and 1200 or 800, 40)
        ui.refresh()
    end

    local function reset()
        elapsed, running, last_cs = 0, false, -1
        ui.refresh()
    end

    local function draw()
        ui.begin("秒表")
        ui.text_center(label(), 92, C.hi)
        ui.text_center(running and "计时中" or "已暂停", 132, C.dim)
        ui.commands({
            { label = "返回", run = ui.back },
            { label = running and "暂停" or "开始", run = toggle },
            { label = "归零", run = reset },
        })
    end

    ui.register({
        id = "stopwatch",
        name = "秒表",
        icon = "stopwatch",
        color = C.hi,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt)
            if not running then return end
            elapsed = elapsed + dt
            local cs = math.floor(elapsed / 100) % 10
            if cs ~= last_cs then
                last_cs = cs
                ui.refresh()
            end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
-- 状态灯：切换熄灭 / 常亮 / 慢闪 / 快闪，离开时关灯。
do
    local C = ui.C
    local MODES = { {label = "熄灭", led = "off"}, {label = "常亮", led = "on"},
                    {label = "慢闪", led = "slow"}, {label = "快闪", led = "fast"} }
    local current = 1

    local function pick(i)
        current = i
        if ui.HAS_LED then ui.led(MODES[i].led) end
        ui.refresh()
    end

    local function draw()
        ui.begin("状态灯")
        if not ui.HAS_LED then
            ui.text_center("本机无 LED 接口", 96, C.dim)
        else
            ui.text_center(MODES[current].label, 92, C.hi)
            screen.rect(85, 128, 70, 40, current == 1 and C.tile_dim or C.hi)
            ui.text_center("请观察实体状态灯", 176, C.dim)
        end

        local items = { { label = "返回", run = function() pick(1); ui.back() end } }
        for i = 1, #MODES do
            items[#items + 1] = { label = MODES[i].label, run = function() pick(i) end }
        end
        ui.commands(items)
    end

    ui.register({
        id = "led",
        name = "状态灯",
        icon = "led",
        color = C.hi,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
-- 待办清单：预置条目 + 单键勾选（没有键盘，不支持自由输入）。
-- 勾选状态以 "0101" 形式的字符串存进桌面共用的存档。
do
    local C = ui.C
    local ITEMS = { "喝水", "起来走走", "回消息", "早点睡" }
    local index = 1
    local done = ""      -- 存档要在 enter 里读：壳层 on_start 之前 store 还是空的

    local function is_done(i)
        return string.sub(done, i, i) == "1"
    end

    local function toggle(i)
        local chars = {}
        for j = 1, #ITEMS do
            chars[j] = (j == i) and (is_done(j) and "0" or "1") or (is_done(j) and "1" or "0")
        end
        done = table.concat(chars)
        ui.save("todo.done", done)
        ui.beep(is_done(i) and 1200 or 700, 40)
        ui.refresh()
    end

    local function draw()
        ui.begin("待办")
        local y = 40
        for i = 1, #ITEMS do
            local on = (i == index)
            if on then screen.rect(6, y - 4, ui.W - 12, 26, C.tile) end
            local mark = is_done(i) and "[x] " or "[ ] "
            ui.text(mark .. ITEMS[i], 12, y, on and C.hi or C.text)
            y = y + 28
        end
        ui.commands({
            { label = "返回", run = ui.back },
            { label = "下一条", run = function()
                index = index % #ITEMS + 1
                ui.refresh()
            end },
            { label = is_done(index) and "取消勾选" or "勾选", run = function() toggle(index) end },
        })
    end

    ui.register({
        id = "todo",
        name = "待办",
        icon = "todo",
        color = C.hi,
        enter = function(self)
            index = 1
            local saved = ui.load("todo.done", "")
            if type(saved) == "string" then done = saved end
            ui.refresh()
        end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
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
-- 设置：桌面手势、蜂鸣开关、清空存档。改动的开关立即写进桌面共用存档。
do
    local C = ui.C

    local function draw()
        ui.begin("设置")
        ui.text("手势: " .. (ui.dwell_on() and "停留打开" or "双击打开"), 12, 44, C.dim)
        ui.text("蜂鸣: " .. (ui.beep_on() and "开" or "关"), 12, 68, C.dim)
        ui.text(string.format("存档: %d 项", ui.save_key_count()), 12, 92, C.dim)
        local note = ui.save_note()
        if note ~= "" then ui.footer(note, C.warn) end

        ui.commands({
            { label = "返回", run = ui.back },
            { label = ui.dwell_on() and "改为双击打开" or "改为停留打开",
              run = function()
                  ui.set_dwell(not ui.dwell_on())
                  ui.notice(ui.dwell_on() and "停留打开" or "双击打开")
              end },
            { label = ui.beep_on() and "关闭蜂鸣" or "打开蜂鸣",
              run = function()
                  local on = not ui.beep_on()
                  ui.set_beep(on)
                  if on then ui.beep(1000, 50) end
                  ui.notice(on and "蜂鸣已开" or "蜂鸣已关")
              end },
            { label = "试听蜂鸣", run = function() ui.beep(880, 120) end },
            { label = "清空存档", run = function()
                  ui.save_clear()
                  ui.notice("存档将在下次写入时清空")
              end },
        })
    end

    ui.register({
        id = "settings",
        name = "设置",
        icon = "settings",
        color = C.dim,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
-- 关于：运行环境信息（API 版本、画布、能力开关、存档用量、当前时间）
do
    local C = ui.C
    local W, H = ui.W, ui.H
    local last_key = ""

    local function capability_line()
        local caps = {}
        caps[#caps + 1] = ui.HAS_LED and "灯" or "无灯"
        caps[#caps + 1] = ui.HAS_BUZZER and "蜂鸣" or "无蜂鸣"
        caps[#caps + 1] = ui.HAS_HTTP and "网络" or "无网络"
        caps[#caps + 1] = ui.HAS_TTS and "播报" or "无播报"
        return table.concat(caps, " ")
    end

    local function draw()
        ui.begin("关于")
        local t = ui.localtime()
        ui.text("LingClaw Lua 桌面", 12, 40, C.text)
        ui.text(string.format("API v%d   画布 %dx%d", app.api_version, W, H), 12, 64, C.dim)
        ui.text("能力: " .. capability_line(), 12, 88, C.dim)
        ui.text(string.format("存档: %d 项", ui.save_key_count()), 12, 112, C.dim)
        if t then
            ui.text(string.format("时间: %04d-%02d-%02d %02d:%02d",
                                  t.year, t.month, t.day, t.hour, t.min), 12, 136, C.dim)
        else
            ui.text("时间: 未校时", 12, 136, C.warn)
        end
        local note = ui.save_note()
        if note ~= "" then ui.footer(note, C.warn) end
        ui.commands({ {label = "返回", run = ui.back} })
    end

    ui.register({
        id = "about",
        name = "关于",
        icon = "about",
        color = C.dim,
        enter = function(self) last_key = ""; ui.refresh() end,
        tick = function(self, dt)
            -- 存档项数/时间会变，变化时才重画
            local t = ui.localtime()
            local k = tostring(ui.save_key_count()) .. ":" .. tostring(t and t.min)
            if k ~= last_key then last_key = k; ui.refresh() end
        end,
        draw = function(self) draw() end,
        leave = function(self) end,
    })
end
-- 一言：联网取一句随机句子（https://v1.hitokoto.cn/，无需密钥）。
-- HTTP 是异步的：先画界面再等结果，并把传输错误 / HTTP 状态 / 业务字段分开判断。
-- 启动阶段不发请求（运行环境会把它登记到安装成功后再发），只靠命令触发刷新。
--
-- 排版：句子长度不可控，所以按"像素宽度"折行（一行 13 个汉字 ≈ 216px，ASCII 减半），
-- 一屏放 4 行，超出的部分滚动查看（右侧有滚动条）。滚动两种入口都给了：
-- 手柄方向键直接翻（壳层的 key 钩子），没有手柄时用命令条里的"翻页"。
do
    local C = ui.C

    local URL = "https://v1.hitokoto.cn/"
    local VISIBLE = 4                  -- 一屏行数（文字额度见下）
    local WRAP_PX = 216                -- 折行宽度：12..228，右侧 233 留给滚动条
    local LINE_H, TOP = 30, 44
    local BAR_X, BAR_Y, BAR_W, BAR_H = 233, 42, 3, 110

    local req_id, state, lines, scroll = nil, "按「换一句」获取", {}, 0

    -- 按"像素宽度"折行：ASCII 8px、其余 16px。必须按字符边界切，切半个 UTF-8
    -- 字符设备会直接报错。长句子不再截断 —— 靠滚动看完。
    local function wrap(s, max_px)
        local out, start, x, i = {}, 1, 0, 1
        while i <= #s do
            local b = string.byte(s, i)
            local n = 1
            local w = 8
            if b >= 0xF0 then n, w = 4, 16
            elseif b >= 0xE0 then n, w = 3, 16
            elseif b >= 0x80 then n, w = 2, 16 end

            if b == 10 then                                  -- 原文里的换行
                out[#out + 1] = string.sub(s, start, i - 1)
                i = i + 1
                start, x = i, 0
            else
                if x + w > max_px and i > start then         -- 放不下就断行
                    out[#out + 1] = string.sub(s, start, i - 1)
                    start, x = i, 0
                end
                x = x + w
                i = i + n
            end
        end
        if start <= #s or #out == 0 then out[#out + 1] = string.sub(s, start) end
        return out
    end

    local function max_scroll()
        local m = #lines - VISIBLE
        return m > 0 and m or 0
    end

    local function page()
        local total = math.ceil(#lines / VISIBLE)
        return math.floor(scroll / VISIBLE) + 1, total > 0 and total or 1
    end

    -- 返回 true 表示这个按键被滚动用掉了（壳层则不再走命令条）
    local function scroll_by(d)
        local m = max_scroll()
        if m == 0 then return false end
        local s = scroll + d
        if s < 0 then s = 0 elseif s > m then s = m end
        if s ~= scroll then scroll = s; ui.refresh() end
        return true
    end

    local function refresh()
        if not ui.HAS_HTTP then state = "本机无网络接口"; ui.refresh(); return end
        req_id = http.get(URL, { timeout_ms = 8000, max_response_bytes = 2048 })
        state = "请求中…"
        ui.refresh()
    end

    local function draw()
        ui.begin("一言")

        for i = 1, VISIBLE do
            local line = lines[scroll + i]
            if line then ui.text(line, 12, TOP + (i - 1) * LINE_H, C.text) end
        end

        -- 滚动条：只在内容超过一屏时出现，位置按 scroll 比例算
        if #lines > VISIBLE then
            screen.rect(BAR_X, BAR_Y, BAR_W, BAR_H, C.line)
            local th = math.max(12, math.floor(BAR_H * VISIBLE / #lines))
            local ty = BAR_Y + math.floor((BAR_H - th) * scroll / max_scroll())
            screen.rect(BAR_X, ty, BAR_W, th, C.hi)
        end

        -- 一行文字兼作状态行：出错/请求中优先，否则给翻页进度与操作提示
        if state ~= "" and state ~= "已更新" then
            ui.footer(state, C.warn)
        elseif #lines > VISIBLE then
            local p, t = page()
            ui.footer(string.format("第 %d/%d 页  方向键翻", p, t), C.dim)
        elseif #lines > 0 then
            ui.footer("方向键翻  A键换一句", C.dim)
        else
            ui.footer(" ", C.dim)
        end

        -- 命令条：没有手柄时靠它翻页/换句（命令条本身也在教用户有哪些操作）
        local cmds = { { label = "返回", run = ui.back },
                       { label = "换一句", run = refresh } }
        if #lines > VISIBLE then
            cmds[#cmds + 1] = { label = "翻页", run = function()
                if not scroll_by(VISIBLE) or scroll + VISIBLE > max_scroll() then
                    scroll = 0        -- 到底了再按就回到开头
                    ui.refresh()
                end
            end }
        end
        ui.commands(cmds)
    end

    ui.register({
        id = "quote",
        name = "一言",
        icon = "quote",
        color = C.warn,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        -- 手柄方向键直接滚动（命令条还在，所以两种输入都能操作）
        key = function(self, id)
            if id == "up" then return scroll_by(-1) end
            if id == "down" then return scroll_by(1) end
            if id == "left" then return scroll_by(-VISIBLE) end
            if id == "right" then return scroll_by(VISIBLE) end
            return false
        end,
        http = function(self, id, response)
            if id ~= req_id then return end
            req_id = nil
            if not response then state = "无响应"; ui.refresh(); return end
            if response.error then
                state = "网络失败: " .. tostring(response.error.code)
                ui.refresh()
                return
            end
            if response.status ~= 200 then
                state = "HTTP " .. tostring(response.status)
                ui.refresh()
                return
            end
            local data = json.decode(response.body)
            if type(data) ~= "table" or type(data.hitokoto) ~= "string" then
                state = "返回格式不认识"
                ui.refresh()
                return
            end
            -- 出处跟在正文后面, 当作最后一行参与折行/滚动 —— 省一段文字额度
            local from = type(data.from) == "string" and #data.from > 0 and data.from or nil
            lines = wrap(from and (data.hitokoto .. "\n—— " .. from) or data.hitokoto, WRAP_PX)
            scroll = 0
            state = "已更新"
            ui.refresh()
        end,
        leave = function(self)
            if req_id and ui.HAS_HTTP then http.cancel(req_id); req_id = nil end
        end,
    })
end
-- 朗读：把预置句子交给设备云端 TTS 播报。
-- 运行环境禁止在启动阶段（源码块 / on_start / 首次 tick）调用 tts.speak，
-- 所以这里只在用户按下"朗读"命令时才请求。
do
    local C = ui.C
    local SENTENCES = {
        "你好，我是桌面里的朗读小应用。",
        "小应用用 Lua 编写，在设备上本地运行。",
        "按功能键可以选择下一条命令，双击执行。",
        "长按功能键三秒可以退出小应用。",
    }
    local index, speech_id, state = 1, nil, "空闲"

    local function speak()
        if not ui.HAS_TTS then state = "本机无播报接口"; return end
        if speech_id then
            tts.cancel(speech_id)
            speech_id = nil
        end
        speech_id = tts.speak(SENTENCES[index])
        state = "请求中"
    end

    local function draw()
        ui.begin("朗读")
        if not ui.HAS_TTS then
            ui.text_center("本机无播报接口", 96, C.dim)
        else
            ui.text(string.format("%d/%d", index, #SENTENCES), 12, 40, C.dim)
            ui.text("…" .. ui.clip(SENTENCES[index], 54), 12, 66, C.text)
            ui.text("状态: " .. state, 12, 110, ui.HAS_TTS and C.hi or C.dim)
            ui.footer("对话/闹钟占用时会被拒绝")
        end
        ui.commands({
            { label = "返回", run = function()
                if speech_id and ui.HAS_TTS then tts.cancel(speech_id); speech_id = nil end
                ui.back()
            end },
            { label = "朗读", run = function() speak(); ui.refresh() end },
            { label = "下一句", run = function()
                index = index % #SENTENCES + 1
                ui.refresh()
            end },
        })
    end

    ui.register({
        id = "speak",
        name = "朗读",
        icon = "speak",
        color = C.hi,
        enter = function(self) ui.refresh() end,
        tick = function(self, dt) end,
        draw = function(self) draw() end,
        tts = function(self, id, result)
            if id ~= speech_id then return end
            speech_id = nil
            if result and result.status == "completed" then
                state = "播报完成"
            elseif result and result.status == "interrupted" then
                state = "被打断"
            else
                local code = result and result.error and result.error.code or ""
                state = (code == "busy") and "对话占用中" or "播报失败"
            end
            ui.refresh()
        end,
        leave = function(self)
            if speech_id and ui.HAS_TTS then tts.cancel(speech_id); speech_id = nil end
        end,
    })
end
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
-- 推箱子：矩形渲染墙/箱子/目标/玩家。方向键走，X 直接跳到下一关（卡住时跳关），
-- 单击功能键撤销；过关后停一拍自动进下一关。共 99 关。
--
-- 关卡由 ./tmp/gen_sokoban.py 生成 —— 反向拉箱保证按构造可解, 再用 A* 正向
-- 求解度量最少推箱步数, 按难度从候选池里挑出 99 关。手写 99 关必然出现无解
-- 的, 而一关无解玩家就永远卡死, 所以关卡数据不手改。
do
    local C = ui.C
    local LEVEL_TOTAL = 99
    local AUTO_STEPS = 50          -- 过关后停 1s (tick 恒 20ms) 再自动进下一关

    local LEVELS = {
        { "#######", "#     #", "#     #", "# #   #", "# $@  #", "#.    #", "#######" },
        { "#######", "#     #", "#     #", "#.##  #", "# $@  #", "#     #", "#######" },
        { "#######", "#  .  #", "#  #$ #", "#   @ #", "#     #", "#     #", "#######" },
        { "#######", "#     #", "#   # #", "#     #", "#  @$ #", "#    .#", "#######" },
        { "#######", "#  .  #", "#  #$ #", "#   @ #", "#     #", "#     #", "#######" },
        { "#######", "#     #", "#     #", "# #.  #", "#  #$ #", "#   @ #", "#######" },
        { "#######", "#     #", "#  #@ #", "# # $ #", "#     #", "#    .#", "#######" },
        { "#######", "#.    #", "#     #", "# $#  #", "# @   #", "#     #", "#######" },
        { "#######", "#   . #", "#@$ # #", "# #   #", "#     #", "#     #", "#######" },
        { "#######", "#     #", "# @   #", "# $ # #", "#     #", "#.    #", "#######" },
        { "#######", "# .   #", "# #   #", "#     #", "#  $  #", "#  @  #", "#######" },
        { "#######", "#   . #", "# @ # #", "# $   #", "#     #", "#     #", "#######" },
        { "########", "#.     #", "#      #", "#    # #", "# $  . #", "#   $# #", "#   @  #", "########" },
        { "########", "#      #", "# #  #.#", "# @$   #", "#.#    #", "#  $   #", "#      #", "########" },
        { "########", "#    @.#", "# $  $ #", "#      #", "#  #   #", "#  # # #", "#  .   #", "########" },
        { "########", "#.     #", "#    $ #", "#      #", "# #    #", "#    $@#", "#     .#", "########" },
        { "########", "#.     #", "#      #", "# # $@ #", "#  # $ #", "#      #", "#     .#", "########" },
        { "########", "#    . #", "#    # #", "# $    #", "#   #  #", "#  $@  #", "#.     #", "########" },
        { "########", "#     .#", "#      #", "# $  # #", "#      #", "# $    #", "#.@    #", "########" },
        { "########", "#      #", "#      #", "# $$#. #", "#  @## #", "#      #", "#.     #", "########" },
        { "########", "#      #", "#   .#.#", "#   #  #", "#   #$ #", "#  $@  #", "#      #", "########" },
        { "########", "#      #", "#    $ #", "# #$   #", "# .@   #", "# # #  #", "#.     #", "########" },
        { "########", "#  .   #", "#  #@  #", "#   $$ #", "#      #", "#   #  #", "#.     #", "########" },
        { "########", "#  .   #", "#  # $ #", "#    $@#", "# #  # #", "#      #", "#.     #", "########" },
        { "########", "#      #", "# @$ $ #", "#      #", "#   ## #", "#      #", "#.    .#", "########" },
        { "########", "#     .#", "#      #", "# $ #  #", "#      #", "#  $@  #", "#.     #", "########" },
        { "########", "#     .#", "#  ##  #", "#      #", "#  $@  #", "# $    #", "#     .#", "########" },
        { "########", "#   @  #", "#   $$ #", "#      #", "#   ## #", "#      #", "#.    .#", "########" },
        { "#########", "#       #", "#.##    #", "# .     #", "# # #   #", "#$  @$  #", "#       #", "#########" },
        { "#########", "#.      #", "#       #", "#    $  #", "#  #    #", "#     $@#", "#      .#", "#########" },
        { "#########", "#.      #", "#   #   #", "#    #  #", "#       #", "# $# $# #", "# @.    #", "#########" },
        { "#########", "#      .#", "# #     #", "#    #  #", "#     $ #", "# # #  $#", "#   .  @#", "#########" },
        { "#########", "#      .#", "#       #", "#  #    #", "#   #   #", "# ##@$  #", "# . $   #", "#########" },
        { "#########", "#    @  #", "# $# $  #", "#       #", "#.#     #", "#  #    #", "#  .    #", "#########" },
        { "#########", "#.      #", "# @  $  #", "# $#    #", "#   #   #", "#    #  #", "#      .#", "#########" },
        { "#########", "#      .#", "#       #", "#   ##  #", "#       #", "#  #$ $@#", "#      .#", "#########" },
        { "#########", "#       #", "# $     #", "#   $   #", "#   @   #", "#     # #", "#.    . #", "#########" },
        { "#########", "#.      #", "#       #", "# # @$  #", "#  #$   #", "#     #.#", "#       #", "#########" },
        { "#########", "#.     .#", "#       #", "#    $@ #", "#    #$ #", "# #     #", "#       #", "#########" },
        { "#########", "#    .  #", "#.#  #  #", "#       #", "#  #    #", "# $#@$  #", "#       #", "#########" },
        { "#########", "#       #", "#     $ #", "#  # $  #", "#    @  #", "# #     #", "# .    .#", "#########" },
        { "#########", "#     . #", "#     #.#", "# $     #", "# @#    #", "# # $ # #", "#       #", "#########" },
        { "#########", "#     . #", "#     # #", "#  #    #", "#.#  #$ #", "#    $@ #", "#       #", "#########" },
        { "#########", "#.      #", "#       #", "#       #", "#  # $  #", "#     $ #", "#.    @ #", "#########" },
        { "#########", "#       #", "#  $    #", "#    #  #", "# $     #", "# @#    #", "#.     .#", "#########" },
        { "#########", "#      .#", "#       #", "#   #   #", "#@$     #", "# $  #  #", "#      .#", "#########" },
        { "#########", "#      .#", "#       #", "#     # #", "#@$     #", "# $     #", "#      .#", "#########" },
        { "#########", "#.      #", "#.#     #", "#       #", "#    $  #", "# # #@$ #", "#       #", "#########" },
        { "##########", "#       .#", "#      # #", "#   # @# #", "#   . $  #", "#   #  $ #", "# $ #    #", "#   .    #", "##########" },
        { "##########", "# .      #", "# #  $ #.#", "#        #", "#        #", "# $ .# $ #", "# @ #  # #", "#        #", "##########" },
        { "##########", "#       .#", "#  #     #", "# $.#    #", "# @ .    #", "#  ##  $ #", "#  $     #", "#        #", "##########" },
        { "##########", "#       .#", "#  $     #", "#     .# #", "#     #  #", "#.#   $  #", "#@$      #", "#        #", "##########" },
        { "##########", "#  .     #", "#  #  #  #", "#    @ # #", "# $  $   #", "#      $ #", "#        #", "#.      .#", "##########" },
        { "##########", "#     .  #", "# # $$#@ #", "# .    $ #", "#.#      #", "# #      #", "#      # #", "#        #", "##########" },
        { "##########", "#.       #", "#        #", "#  .# #$ #", "#  #*#   #", "#        #", "#@$ #    #", "#        #", "##########" },
        { "##########", "#.       #", "# $#   # #", "#  @$  # #", "# $#     #", "#        #", "#.#      #", "#       .#", "##########" },
        { "##########", "#  .     #", "#  #   $@#", "# #      #", "# $  #   #", "# $    # #", "#.#      #", "#       .#", "##########" },
        { "##########", "#.       #", "#   @$   #", "# $#     #", "#$ #     #", "#  # #   #", "#    #   #", "#.      .#", "##########" },
        { "##########", "# ..     #", "# ##   $ #", "#        #", "#      $ #", "#   @$ #.#", "#        #", "#        #", "##########" },
        { "##########", "#    .   #", "# $  # $ #", "#    # @ #", "#     #$ #", "#   #    #", "#   #    #", "#.  .    #", "##########" },
        { "##########", "#        #", "#        #", "#.##     #", "#     $  #", "#   $#@$ #", "#     #  #", "#.    .  #", "##########" },
        { "##########", "#.      .#", "#        #", "#        #", "#   # $  #", "#    $ #.#", "#    @$  #", "#        #", "##########" },
        { "##########", "#.      .#", "#        #", "#  #     #", "#      @ #", "#   # $$ #", "# $ #    #", "#.       #", "##########" },
        { "##########", "#       .#", "#      #.#", "#   #    #", "#        #", "#   $    #", "# # $    #", "# .$@    #", "##########" },
        { "##########", "#.       #", "#  $@$  $#", "# #      #", "#        #", "#        #", "#  #     #", "#.      .#", "##########" },
        { "##########", "#        #", "# # $  # #", "#@$      #", "# $#     #", "#  .#  #.#", "#      # #", "#      . #", "##########" },
        { "##########", "#.      .#", "#        #", "#    #   #", "#    #   #", "#        #", "#  $ $$  #", "#     @ .#", "##########" },
        { "##########", "# .     .#", "# #.     #", "#  #  ## #", "#        #", "#     $  #", "#    $@$ #", "#        #", "##########" },
        { "##########", "#        #", "#        #", "# $      #", "# @#  .#.#", "# $#$ ## #", "#        #", "#       .#", "##########" },
        { "##########", "#       .#", "#        #", "#  $     #", "#        #", "#.#$     #", "#@$  #   #", "#       .#", "##########" },
        { "##########", "#       .#", "# $ # #  #", "# $      #", "# @$     #", "#     #  #", "#.##     #", "#       .#", "##########" },
        { "##########", "#     . .#", "#     #  #", "#     #  #", "# $ #    #", "#        #", "# $$     #", "# @     .#", "##########" },
        { "##########", "#.       #", "# #   #  #", "#  # $+$ #", "#     #  #", "#      $ #", "#   #    #", "#   .   *#", "##########" },
        { "##########", "#.   .   #", "#    #   #", "# $ #    #", "#  $     #", "#.#+# $  #", "#  $ #   #", "#        #", "##########" },
        { "##########", "#       .#", "#     $@$#", "#     #$ #", "#  .#    #", "#  #     #", "# #*#    #", "# .      #", "##########" },
        { "##########", "#.       #", "#       $#", "#        #", "#  $#  # #", "# $@$  . #", "#  #   #.#", "#  .     #", "##########" },
        { "##########", "#   .    #", "#  .#    #", "# $#   #.#", "# @#     #", "# $   $  #", "#   # $  #", "#   .    #", "##########" },
        { "##########", "#        #", "# $  $   #", "#   #    #", "# ##     #", "#  *@#   #", "# $# #   #", "#. .    .#", "##########" },
        { "##########", "#    . . #", "#    # # #", "# #   $  #", "#        #", "# $ # $  #", "#    # $@#", "#    .  .#", "##########" },
        { "##########", "#.       #", "#        #", "#     ##.#", "#        #", "#  $ #   #", "#  $@#$$ #", "#.   .   #", "##########" },
        { "##########", "#.      .#", "#        #", "#    @   #", "#    $   #", "# #    # #", "#  $ $ #$#", "#      ..#", "##########" },
        { "##########", "#.   @  .#", "#    $   #", "#     $  #", "#    #.  #", "#    ##$ #", "#      $ #", "#.       #", "##########" },
        { "##########", "#.      .#", "#    $ $ #", "#        #", "#   @$ $ #", "#  #     #", "#     #  #", "#.    .  #", "##########" },
        { "##########", "#.      .#", "#   #.#  #", "# #      #", "#      $ #", "#$#   #@$#", "#      $ #", "#.       #", "##########" },
        { "##########", "# .    . #", "#.#  $ # #", "#    $ # #", "#      #.#", "#        #", "#    $#$ #", "#    @   #", "##########" },
        { "##########", "#  .    .#", "#  #     #", "#        #", "# $  @   #", "#    $ # #", "# $$     #", "#.      .#", "##########" },
        { "##########", "#     .  #", "# $ $$## #", "#      @ #", "#      $ #", "#        #", "#      #.#", "#.     . #", "##########" },
        { "##########", "#     . .#", "# $   #  #", "# $  .#  #", "#    #   #", "#   #.   #", "# $  $@  #", "#        #", "##########" },
        { "##########", "#       .#", "#  $ $ $ #", "#     #@ #", "#      $ #", "#        #", "#      #.#", "#.      .#", "##########" },
        { "##########", "#       .#", "# $ $ $  #", "#    @$  #", "#  #  #  #", "#  #.    #", "#  .#  # #", "#.       #", "##########" },
        { "##########", "#.   .  .#", "#@$  #   #", "# $  #   #", "#    #   #", "# $      #", "#  $     #", "#.       #", "##########" },
        { "##########", "#       .#", "# $##    #", "# $      #", "#        #", "#@$  .#  #", "#    # $ #", "#    .  .#", "##########" },
        { "##########", "#.      .#", "#  #   #.#", "# $ $    #", "#  @     #", "# $$     #", "# #.#    #", "#        #", "##########" },
        { "##########", "#.      .#", "#        #", "#        #", "#    $   #", "#  ##@$$ #", "#    $   #", "#.      .#", "##########" },
        { "##########", "#.    .  #", "#     ## #", "#    #   #", "#   #@ # #", "#    $#. #", "#  $$  $ #", "#       .#", "##########" },
        { "##########", "#.      .#", "#  ##    #", "#     #  #", "#    $   #", "#  @$    #", "#  # $$# #", "#. .     #", "##########" },
        { "##########", "#       .#", "#      # #", "#.###  . #", "#      #.#", "# @$   $ #", "# $$     #", "#        #", "##########" },
        { "##########", "#..      #", "# #    # #", "#  #     #", "#  # # $ #", "#      $@#", "#  $ # $ #", "#.   .   #", "##########" },
        { "##########", "#       .#", "#  $@    #", "# $#     #", "#    .#  #", "# $ ##.  #", "# $      #", "#       .#", "##########" },
    }

    local level, grid, player, moves, history, state, auto_left, solved_now

    local function load_level(n)
        if n > LEVEL_TOTAL then n = LEVEL_TOTAL end
        if n < 1 then n = 1 end
        level = n
        grid = {}
        for r, row in ipairs(LEVELS[level]) do
            grid[r] = {}
            for c = 1, #row do
                local ch = string.sub(row, c, c)
                grid[r][c] = {
                    wall = ch == "#",
                    goal = ch == "." or ch == "*" or ch == "+",
                    box = ch == "$" or ch == "*",
                }
                if ch == "@" or ch == "+" then player = {r = r, c = c} end
            end
        end
        moves, history, state, auto_left, solved_now = 0, {}, "playing", 0, 0
        ui.save("sokoban.level", level)
    end

    local function in_bounds(r, c)
        return grid[r] and grid[r][c]
    end

    local function solved()
        for r = 1, #grid do
            for c = 1, #grid[r] do
                local t = grid[r][c]
                if t.box and not t.goal then return false end
            end
        end
        return true
    end

    local function next_level()
        ui.led("off")
        if level >= LEVEL_TOTAL then
            load_level(1)              -- 打完 99 关从头再来
            ui.notice("全部通关，回到第 1 关")
        else
            load_level(level + 1)
            ui.notice(string.format("第 %d 关", level))
        end
        ui.refresh()
    end

    local function restart()
        ui.led("off")
        load_level(level)
        ui.refresh()
    end

    local function step(dr, dc)
        if state ~= "playing" then return end
        local nr, nc = player.r + dr, player.c + dc
        local next = in_bounds(nr, nc)
        if not next or next.wall then
            ui.beep(300, 40)
            return
        end
        local pushed_from, pushed_to = nil, nil
        if next.box then
            local br, bc = nr + dr, nc + dc
            local beyond = in_bounds(br, bc)
            if not beyond or beyond.wall or beyond.box then
                ui.beep(300, 40)
                return
            end
            next.box, beyond.box = false, true
            pushed_from, pushed_to = {r = nr, c = nc}, {r = br, c = bc}
        end
        history[#history + 1] = {dr = dr, dc = dc,
                                 pushed_from = pushed_from, pushed_to = pushed_to}
        player = {r = nr, c = nc}
        moves = moves + 1
        ui.beep(900, 20)
        if solved() then
            state, auto_left, solved_now = "done", AUTO_STEPS, moves
            ui.led("slow")
        end
        ui.refresh()
    end

    local function undo()
        if state == "done" then
            -- 撤掉最后一步 = 回到未过关状态, 顺手取消自动跳关
            state, auto_left = "playing", 0
            ui.led("off")
        end
        local h = table.remove(history)
        if not h then
            ui.beep(300, 40)
            return
        end
        if h.pushed_to then
            local to = in_bounds(h.pushed_to.r, h.pushed_to.c)
            if to then to.box = false end
            local from = in_bounds(h.pushed_from.r, h.pushed_from.c)
            if from then from.box = true end
        end
        player = {r = player.r - h.dr, c = player.c - h.dc}
        moves = math.max(0, moves - 1)
        ui.refresh()
    end

    local function draw()
        ui.begin("推箱子")
        local rows = #grid
        local cols = #grid[1]
        local size = math.floor(math.min((ui.W - 8) / cols, (ui.H - 96) / rows))
        local ox = math.floor((ui.W - cols * size) / 2)
        local oy = 40

        for r = 1, rows do
            for c = 1, cols do
                local t = grid[r][c]
                local x, y = ox + (c - 1) * size, oy + (r - 1) * size
                if t.wall then
                    screen.rect(x, y, size, size, C.line)
                else
                    screen.rect(x, y, size, size, 0x121E26)
                    if t.goal then
                        screen.rect(x + math.floor(size / 2) - 2, y + math.floor(size / 2) - 2,
                                    4, 4, C.warn)
                    end
                    if t.box then
                        screen.rect(x + 2, y + 2, size - 4, size - 4,
                                    t.goal and C.hi or C.warn)
                    end
                    if player.r == r and player.c == c then
                        screen.rect(x + math.floor(size / 3), y + math.floor(size / 3),
                                    math.max(2, size - 2 * math.floor(size / 3)),
                                    math.max(2, size - 2 * math.floor(size / 3)), C.text)
                    end
                end
            end
        end

        ui.text(string.format("第 %d/%d 关   步数 %d%s", level, LEVEL_TOTAL, moves,
                              state == "done" and " · 完成" or ""), 12, 32, C.text)
        if state == "playing" then
            -- 游玩中让出命令条: 方向键直接走人, X 跳关, 单击撤销
            ui.commands_clear()
            ui.footer("方向键走 X跳关 单击撤销")
        else
            ui.commands({
                { label = "重开", run = restart },
                { label = "下一关", run = next_level },
                { label = "返回", run = function() ui.led("off"); ui.back() end },
            })
        end
    end

    ui.register({
        id = "sokoban",
        name = "推箱子",
        icon = "sokoban",
        color = C.warn,
        raw = true,
        enter = function(self)
            local n = ui.load("sokoban.level", 1)
            if type(n) ~= "number" then n = 1 end
            load_level(n)
            ui.refresh()
        end,
        tick = function(self, dt)
            if state ~= "done" or auto_left <= 0 then return end
            auto_left = auto_left - 1
            if auto_left == 0 then next_level() end
        end,
        draw = function(self) draw() end,
        click = function(self, id)
            if id == "up" then step(-1, 0)
            elseif id == "down" then step(1, 0)
            elseif id == "left" then step(0, -1)
            elseif id == "right" then step(0, 1)
            elseif id == "function_double" then next_level()   -- 手柄 叉/Cross
            elseif id == "function" then undo()                -- 单击功能键
            end
        end,
        leave = function(self) ui.led("off") end,
    })
end
