-- API 4 TTS: busy, cancellation and completion chaining.
assert(app.api_version>=4 and tts,"TTS: API 4 required")
local current, overlap, cancelled, elapsed = nil,nil,nil,0
local state, detail, stage = "等待播报", "", 0
local function draw()
  screen.begin(0x101923)
  screen.text("应用内播报",12,12,0xFFFFFF)
  screen.text(state,12,48,0x71E4AD)
  screen.text(detail,12,80,0xA5B5C5)
  screen.text("单击播报，双击取消",12,140,0xFFFFFF)
  screen.text("唤醒或闹钟可打断",12,172,0xA5B5C5)
  screen.present()
end
local function speak()
  stage=1
  current=tts.speak("这是小应用的语音测试。播完后会继续下一句。")
  overlap=tts.speak("这句话不应排队播放。")
  state="正在请求";detail="";draw()
end
function on_start()draw()end
function on_tick(dt)
  elapsed=elapsed+dt
  if stage==0 and elapsed>=1500 then speak()end
end
function on_tts_result(id,result)
  assert(id~=cancelled,"TTS: callback after cancel")
  if id==overlap then
    assert(result.status=="failed" and result.error and result.error.code=="busy","TTS: overlap must fail busy")
    overlap=nil;return
  end
  assert(id==current,"TTS: stale callback")
  current=nil;state=result.status;detail=result.error and result.error.code or ""
  if result.status=="completed" and stage==1 then
    stage=2;current=tts.speak("连续播报成功。")
  else stage=3 end
  draw()
end
local last_click=-1000
function on_button_click(id)
  if id~="function" then return end
  if elapsed-last_click<=300 and current then
    cancelled=current;assert(tts.cancel(current));assert(not tts.cancel(current));current=nil
    state="已取消";stage=3;draw()
  elseif not current then speak()end
  last_click=elapsed
end
