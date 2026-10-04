/*
 * ss_screen.h - auto blanking + event wake for the SoundSense build
 */
#ifndef SS_SCREEN_H
#define SS_SCREEN_H

#include "ss_core.h"

/* Start the blanking manager (1 s ticker task). */
int ss_screen_init(void);

/* Refresh activity (button/voice/any interaction): un-blank + restart timer. */
void ss_screen_activity(void);

/* Detection event arrived: un-blank and show an alert face. */
void ss_screen_on_event(const ss_event_t *ev);

#endif /* SS_SCREEN_H */
