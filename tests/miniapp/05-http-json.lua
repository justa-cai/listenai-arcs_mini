-- API 4 contract test using the public, credential-free https://httpbingo.org service.
local BASE_URL = "https://httpbingo.org"
assert(app.api_version >= 4 and http and json, "HTTP: API 4 required")
local pending, step, passed, status = nil, 0, 0, "准备请求"
local cases = {
  {path="/base64/eyJuZXN0ZWQiOnsidmFsdWUiOjd9LCJpdGVtcyI6W251bGwsZmFsc2VdfQ==?content-type=application%2Fjson", check=function(r)
    assert(r.status==200 and r.content_type=="application/json", "HTTP GET: status="..tostring(r.status)..", content_type="..tostring(r.content_type)..", error="..tostring(r.error and r.error.code))
    local d=assert(json.decode(r.body));assert(d.nested.value==7 and d.items[1]==json.null and d.items[2]==false)
    assert(json.decode(json.encode(d)).nested.value==7)
  end},
  {path="/post", method="POST", headers={["Content-Type"]="text/plain"}, body=string.rep("x",2048), check=function(r)
    assert(r.status==200);local d=assert(json.decode(r.body));assert(d.data==string.rep("x",2048))
  end},
  {path="/status/404", check=function(r) assert(r.status==404 and not r.error) end},
  {path="/redirect-to?url=%2Fget", check=function(r) assert(r.status==302) end},
  {path="/bytes/1024", max_response_bytes=64, check=function(r) assert(r.error.code=="response_too_large" and not r.body) end},
  {path="/delay/3", timeout_ms=1000, check=function(r) assert(r.error.code=="timeout") end},
  {path="/stream/2", check=function(r)
    assert(r.status==200);local count=0
    for line in r.body:gmatch("[^\r\n]+") do
      local d=assert(json.decode(line));assert(d.id==count);count=count+1
    end
    assert(count==2)
  end},
}
local function draw()
  screen.begin(0x101923)
  screen.text("HTTP / JSON",12,12,0xFFFFFF)
  screen.text("通过 "..passed.." / "..#cases,12,48,0x71E4AD)
  screen.text(status,12,80,0xFFFFFF)
  screen.text("单击重新测试",12,160,0xA5B5C5)
  screen.present()
end
local function next_case()
  step=step+1
  local c=cases[step]
  if not c then status="全部通过";draw();return end
  status="请求 "..step;draw()
  pending=http.request({url=BASE_URL..c.path,method=c.method,headers=c.headers,body=c.body,timeout_ms=c.timeout_ms,max_response_bytes=c.max_response_bytes})
  assert(type(pending)=="number", "HTTP: request ID must be numeric")
end
function on_start()
  assert(json.encode({})=="{}", "JSON: empty object must remain {}")
  assert(json.encode(json.decode("[]"))=="[]", "JSON: empty array must remain []")
  local d,e=json.decode("invalid");assert(d==nil and e=="invalid_json", "JSON: invalid input must return nil, invalid_json")
  local id=http.get(BASE_URL.."/delay/3");assert(http.cancel(id));assert(not http.cancel(id))
  next_case() -- staged during startup; sent once after successful installation
end
function on_http_response(id,r)
  assert(id==pending,"HTTP: stale or cancelled callback")
  pending=nil;cases[step].check(r);passed=passed+1;next_case()
end
function on_button_click(id)
  if id~="function" or pending then return end
  step,passed=0,0;next_case()
end
function on_tick()end
