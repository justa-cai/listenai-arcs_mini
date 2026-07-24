#define TAG "intent_prompt_tone"

#include "voice_intent_prompt_tone.h"

#include "lisa_log.h"
#include "voice_intent_mgr.h"
#include "voice_player/voice_player_tts.h"

static void prompt_tone_intent_on_enter(void)
{
    LOGI("PROMPT_TONE on_enter");
    /* TTS inactive 时可能仍保存着 PHOTO_FLOW 复播快照，不重复 stop。 */
    if (voice_player_tts_is_active()) {
        voice_player_tts_stop();
    }
}

static void prompt_tone_intent_on_resumed(void)
{
    LOGI("PROMPT_TONE on_resumed");
}

static void prompt_tone_intent_on_exit(void)
{
    LOGI("PROMPT_TONE on_exit");
}

int voice_intent_prompt_tone_register(void)
{
    return voice_intent_register_ops(&(voice_intent_ops_t){
        .type = INTENT_PROMPT_TONE,
        .on_enter = prompt_tone_intent_on_enter,
        .on_resumed = prompt_tone_intent_on_resumed,
        .on_exit = prompt_tone_intent_on_exit,
    });
}
