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
