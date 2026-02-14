#ifndef __MUSIC_MANAGER_H__
#define __MUSIC_MANAGER_H__

#include "audio_out.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MUSIC_SOURCE_NONE,
    MUSIC_SOURCE_LOCAL,
    MUSIC_SOURCE_ONLINE
} music_source_t;

typedef struct {
    bool is_active;
    music_source_t source;
    bool auto_next;
    char last_keyword[128];
} music_manager_state_t;

void music_manager_init(void);

void music_manager_set_active(bool active);
void music_manager_set_source(music_source_t source);
void music_manager_set_auto_next(bool enable);
void music_manager_clear_keyword(void);

bool music_manager_is_active(void);
music_source_t music_manager_get_source(void);

int music_manager_play_next_random(char *song_name, int name_len);
int music_manager_fetch_and_play_random(char *song_name, int name_len);
int music_manager_search_and_play(const char *keyword, char *song_name, int name_len);

#endif
