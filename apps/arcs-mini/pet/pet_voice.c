/*
 * pet_voice.c - pet voice line playback (tone partition mp3s)
 */
#include "pet_voice.h"

#include "app_tone.h"
#include "tone_control/voice_player_tone.h"

void pet_voice_play(uint8_t tone_id)
{
    if (tone_id == PET_TONE_NONE) {
        return;
    }
    const char *url = app_tone_get_url(tone_id);
    if (!url) {
        return;
    }
    voice_player_play_tone_url(url);
}
