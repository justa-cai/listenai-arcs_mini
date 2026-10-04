/*
 * pet_ui.h - full-screen pet page (default screen)
 */
#ifndef PET_UI_H
#define PET_UI_H

/* Register the pet nav screen (does not open it). UI must be up. */
int pet_ui_init(void);

/* Make the pet page the default screen (marshalled to the UI thread). */
int pet_ui_become_default(void);

/* Show a centered toast line for ~2 s (e.g. action result / reminder). */
void pet_ui_toast(const char *text);

/* Push an immediate widget refresh (used after debug/shell mutations). */
void pet_ui_kick(void);

#endif /* PET_UI_H */
