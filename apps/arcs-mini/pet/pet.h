/*
 * pet.h - virtual pet module facade
 */
#ifndef PET_H
#define PET_H

/* Bring up the pet: state machine, UI page (as default screen) and buzzer.
 * Called from main() after lisa_ui_init(). */
int pet_init(void);

#endif /* PET_H */
