-- 网络探测：DNS + TCP/TLS 到 Muse 域名（旁路由生效性验证）
local COL = { bg = 0x101923, text = 0xFFFFFF, hi = 0x7E9CFF, dim = 0xA5B5C5, warn = 0xFFB84D }
local results = {}
local reqs = {}
local TARGETS = {
    { name = "api.muse.ai",       url = "https://api.muse.ai/" },
    { name = "hatch.metaaivm.com", url = "https://hatch.metaaivm.com/" },
    { name = "github.com",        url = "https://github.com/" },
    { name = "baidu(对照)",       url = "http://www.baidu.com/" },
}
local dirty = true

local function refresh() dirty = true end

local function draw()
    screen.begin(COL.bg)
    screen.rect(0, 0, 240, 26, 0x1C2A3A)
    screen.text("网络探测", 10, 5, COL.text)
    local y = 44
    for i = 1, #TARGETS do
        local r = results[i]
        local line = TARGETS[i].name .. ": " .. (r or "…")
        screen.text(line, 12, y, r == nil and COL.dim or
                    (r:sub(1, 1) == "✓" and COL.hi or COL.warn))
        y = y + 28
    end
    screen.present()
end

function on_start()
    for i, t in ipairs(TARGETS) do
        local id = http.get(t.url, { timeout_ms = 8000, max_response_bytes = 256 })
        reqs[id] = i
    end
    refresh()
end

function on_http_response(id, resp)
    local i = reqs[id]
    if not i then return end
    reqs[id] = nil
    if not resp or resp.error then
        results[i] = "✗ " .. (resp and resp.error and resp.error.code or "no-resp")
    else
        results[i] = "✓ HTTP " .. tostring(resp.status)
    end
    refresh()
end

function on_tick()
    if dirty then dirty = false; draw() end
end
