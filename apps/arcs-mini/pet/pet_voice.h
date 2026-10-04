/*
 * pet_voice.h - pet voice line playback (tone partition mp3s)
 *
 * All lines are pre-synthesized fixed-text clips in res/arcs-mini/tone/
 * (106..131, "soft milky cute little kid" voice). Dynamic texts with live
 * numbers stay on the cloud TTS path (pet_care replies are read by the
 * cloud while online; these clips cover offline and device-initiated lines).
 */
#ifndef PET_VOICE_H
#define PET_VOICE_H

#include <stdint.h>

/* tone ids, keep in sync with apps/arcs-mini/tone/ file names */
enum {
    PET_TONE_NONE = 255,
    PET_TONE_EAT_OK = 106,
    PET_TONE_SNACK_OK = 107,
    PET_TONE_FULL = 108,
    PET_TONE_CLEAN_OK = 109,
    PET_TONE_CLEAN_NONE = 110,
    PET_TONE_PLAY_OK = 111,
    PET_TONE_TIRED = 112,
    PET_TONE_SLEEP_OK = 113,
    PET_TONE_WAKE_OK = 114,
    PET_TONE_NOT_SLEEPY = 115,
    PET_TONE_MED_OK = 116,
    PET_TONE_MED_NONE = 117,
    PET_TONE_SLEEPING = 118,
    PET_TONE_STATUS = 119,
    PET_TONE_EGG_TOUCH = 120,
    PET_TONE_GREET = 121,
    PET_TONE_LOVE = 122,
    PET_TONE_PAT = 123,
    PET_TONE_HATCH = 124,
    PET_TONE_EVOLVE = 125,
    PET_TONE_R_HUNGRY = 126,
    PET_TONE_R_SAD = 127,
    PET_TONE_R_DIRTY = 128,
    PET_TONE_R_TIRED = 129,
    PET_TONE_R_SICK = 130,
    PET_TONE_R_MISS = 131,
};

/* Queue one clip on the tone player (safe from any thread). */
void pet_voice_play(uint8_t tone_id);

#endif /* PET_VOICE_H */
