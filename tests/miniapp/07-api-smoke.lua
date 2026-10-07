-- API 冒烟自检 (API 4)
--
-- 把 miniapp 运行环境里所有公开接口都调一遍，逐项在屏幕上显示 OK / FAIL / --。
-- 只做合法调用：运行环境没有 pcall/xpcall，任何 Lua 异常都会直接终止应用。
-- 异步接口 (http / tts) 不能在启动阶段触发，改为启动约 2 秒后自动发起一次，
-- 之后按功能键可以重跑。
--
-- 上传： adb push tests/miniapp/07-api-smoke.lua /miniapp/
-- 操作： 单击功能键翻页 (共 3 页)，最后一页单击重跑 http / tts。
--       长按功能键 3 秒退出。

local ROWS = 7              -- 每页行数 (8 段文字上限, 1 段留给标题)
local results = {}          -- { {name=, state="OK"|"FAIL"|"--", info=}, ... }
local page = 0
local http_row, tts_row     -- 异步结果在 results 里的下标
local http_id, tts_id
local dirty = false

-- 截断到 <=63 字节且不切坏 UTF-8 (screen.text 超出上限会抛异常)
local function clip(s, limit)
    if #s <= limit then return s end
    local cut = limit
    while cut > 0 do
        local b = string.byte(s, cut + 1)
        if b and b >= 0x80 and b < 0xC0 then cut = cut - 1 else break end
    end
    return string.sub(s, 1, cut)
end

local function add(name, state, info)
    results[#results + 1] = {name = name, state = state, info = info or ""}
end

local function draw()
    screen.begin(0x101820)
    screen.rect(0, 0, app.width, 24, 0x1E2E3E)
    screen.text(string.format("API 自检 %d/%d", page + 1, (#results + ROWS - 1) // ROWS),
        8, 4, 0x66DDEE)

    local first = page * ROWS
    for i = 0, ROWS - 1 do
        local r = results[first + i + 1]
        local y = 30 + i * 26
        if not r then
            screen.text("-", 8, y, 0x404850)
        else
            local color = 0xAAAAAA
            if r.state == "OK" then color = 0x55DD88
            elseif r.state == "FAIL" then color = 0xFF6666 end
            local line = r.name .. " " .. r.state
            if r.info ~= "" then line = line .. " " .. r.info end
            screen.text(clip(line, 63), 8, y, color)
        end
    end
    screen.present()
end

-- ---------------------------------------------------------------- 同步检查

local function check_app()
    local v = app.api_version
    add("app.api_version", v == 4 and "OK" or "FAIL", tostring(v))
    add("app.size", (app.width == 240 and app.height == 240) and "OK" or "FAIL",
        tostring(app.width) .. "x" .. tostring(app.height))
end

local function check_sandbox()
    -- 文档声明这些不可用；出现任何一个都说明沙箱裁剪没生效
    local leaked = {}
    for _, n in ipairs({"io", "os", "package", "debug", "coroutine",
                        "print", "pcall", "xpcall", "load", "dofile",
                        "loadfile", "collectgarbage", "setmetatable"}) do
        if _G[n] ~= nil then leaked[#leaked + 1] = n end
    end
    if #leaked == 0 then add("沙箱裁剪", "OK", "")
    else add("沙箱裁剪", "FAIL", table.concat(leaked, ",")) end
end

local function check_screen()
    local ok = type(screen.begin) == "function" and type(screen.rect) == "function"
           and type(screen.text) == "function" and type(screen.present) == "function"
    add("screen 接口", ok and "OK" or "FAIL", "")
end

local function check_json()
    local have = type(json.encode) == "function" and type(json.decode) == "function"
    add("json 接口", have and "OK" or "FAIL", "")
    if not have then return end

    -- 编码：对象 → 字符串
    local enc = json.encode({a = 1, b = "x"})
    add("json.encode", (type(enc) == "string" and string.find(enc, "\"a\"") ~= nil)
        and "OK" or "FAIL", type(enc))

    -- 解码：字符串 → 表，字段值正确
    local dec = json.decode("{\"n\":2,\"s\":\"ok\",\"b\":true}")
    add("json.decode", (type(dec) == "table" and dec.n == 2 and dec.s == "ok"
        and dec.b == true) and "OK" or "FAIL", type(dec))

    -- 数组往返
    local arr = json.decode("[1,2,3]")
    add("json 数组", (type(arr) == "table" and arr[1] == 1 and arr[3] == 3)
        and "OK" or "FAIL", "")

    -- null 哨兵：lightuserdata，不是 nil
    add("json.null", type(json.null) == "userdata" and "OK" or "FAIL",
        type(json.null))
end

local function check_clock()
    local now = clock.now()
    add("clock.now", type(now) == "number" and "OK" or "--",
        type(now) == "number" and "已校时" or "未校时")

    local t = clock.localtime()
    if type(t) ~= "table" then
        add("clock.localtime", "--", "未校时")
        return
    end
    local ok = type(t.year) == "number" and type(t.month) == "number"
           and type(t.hour) == "number" and type(t.min) == "number"
           and type(t.utc_offset) == "number"
    add("clock.localtime", ok and "OK" or "FAIL",
        string.format("%02d:%02d", t.hour, t.min))
end

local function check_out()
    local lok = type(led.on) == "function" and type(led.off) == "function"
            and type(led.blink) == "function"
    add("led 接口", lok and "OK" or "FAIL", "")
    if lok then led.blink("status", 200, 200) end   -- 观察实体灯是否闪

    local bok = type(buzzer.play) == "function"
    add("buzzer 接口", bok and "OK" or "FAIL", "")
    if bok then buzzer.play(880, 80) end            -- 观察是否出声

    -- 整曲播放: 一小段"哆唻咪", 声音应当是连的(没有音与音之间的空隙)
    local sok = type(buzzer.play_seq) == "function"
    add("buzzer.play_seq 接口", sok and "OK" or "FAIL", "")
    if sok then buzzer.play_seq("523:200,587:200,659:400") end
end

local function check_storage()
    local have = type(storage.save) == "function" and type(storage.load) == "function"
           and type(storage.clear) == "function"
    add("storage 接口", have and "OK" or "FAIL", "")
    if not have then return end

    local probe = 20261007
    local saved, reason = storage.save({probe = probe, tag = "smoke"})
    if saved ~= true then
        -- 未校时属环境问题，不算固件缺陷
        add("storage.save", reason == "clock_unavailable" and "--" or "FAIL",
            tostring(reason))
        add("storage.load", "--", "跳过")
        add("storage.clear", "--", "跳过")
        return
    end
    add("storage.save", "OK", "")

    local back = storage.load()
    add("storage.load", (type(back) == "table" and back.probe == probe
        and back.tag == "smoke") and "OK" or "FAIL", type(back))

    local cleared, creason = storage.clear()
    add("storage.clear", cleared == true and "OK" or "FAIL", tostring(creason))
end

local function check_async_api()
    local hok = type(http.request) == "function" and type(http.get) == "function"
            and type(http.cancel) == "function"
    add("http 接口", hok and "OK" or "FAIL", "")
    local tok = type(tts.speak) == "function" and type(tts.cancel) == "function"
    add("tts 接口", tok and "OK" or "FAIL", "")
end

-- ---------------------------------------------------------------- 异步检查

local function trigger_async()
    -- 重新发起：复用已有行，避免 results 无限增长
    if not http_row then
        add("http 请求", "--", "")
        http_row = #results
        add("tts 播报", "--", "")
        tts_row = #results
    end
    results[http_row].state, results[http_row].info = "--", ""
    results[tts_row].state, results[tts_row].info = "--", ""

    -- http 在启动阶段只是登记、不真正发送，所以从按键触发
    http_id = http.get("http://httpbingo.org/get", {timeout_ms = 8000})

    -- tts 不允许在启动/退出时机请求 (固件在 validating/retiring 会抛异常)
    tts_id = tts.speak("API 自检完成")
    dirty = true
end

function on_http_response(request_id, response)
    if request_id ~= http_id or not http_row then return end
    if response.error then
        results[http_row].state = "--"              -- 网络不可达属环境问题
        results[http_row].info = tostring(response.error.code)
    else
        results[http_row].state = response.status == 200 and "OK" or "FAIL"
        results[http_row].info = "status=" .. tostring(response.status)
    end
    dirty = true
end

function on_tts_result(speech_id, result)
    if speech_id ~= tts_id or not tts_row then return end
    local s = result.status
    if s == "completed" then
        results[tts_row].state = "OK"
    elseif s == "failed" then
        local code = result.error and result.error.code or ""
        results[tts_row].state = (code == "busy") and "--" or "FAIL"
        results[tts_row].info = tostring(code)
    else
        results[tts_row].state = "--"               -- interrupted
        results[tts_row].info = tostring(s)
    end
    dirty = true
end

-- ---------------------------------------------------------------- 生命周期

function on_start()
    check_app()
    check_sandbox()
    check_screen()
    check_json()
    check_clock()
    check_out()
    check_storage()
    check_async_api()
    draw()
end

local ticks = 0
function on_tick(dt_ms)
    -- 启动约 2 秒后自动跑一次异步检查 (启动阶段不允许请求 http/tts)
    ticks = ticks + 1
    if ticks == 100 and not http_row then trigger_async() end
    if dirty then
        dirty = false
        draw()
    end
end

function on_button_click(button_id)
    if button_id ~= "function" then return end
    local last = (#results + ROWS - 1) // ROWS - 1
    if page < last then
        page = page + 1
        if page == last and not http_row then trigger_async() end
    else
        trigger_async()                             -- 最后一页：重跑异步检查
    end
    draw()
end

function on_exit()
    led.off("status")
end
