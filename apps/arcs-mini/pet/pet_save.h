/*
 * pet_save.h - pet state persistence (lisa_kv)
 */
#ifndef PET_SAVE_H
#define PET_SAVE_H

#include <stdbool.h>

#include "pet_core.h"

/* Write the persist blob (with magic/version/CRC). */
void pet_save_write(const pet_persist_t *state);

/* Read and validate. Returns false when no valid save exists (caller starts
 * a fresh egg). */
bool pet_save_read(pet_persist_t *state);

#endif /* PET_SAVE_H */
