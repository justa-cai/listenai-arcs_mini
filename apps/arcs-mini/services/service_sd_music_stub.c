/* Stub implementations when SD card is disabled (no hardware support) */
#include <stdbool.h>

int service_sd_music_init(void) { return 0; }
int service_sd_music_scan(const char *path) { (void)path; return -1; }
bool service_sd_music_is_syncing(void) { return false; }
bool service_sd_music_is_card_ready(void) { return false; }
