/*
 * pet_draw.h - procedural pet rendering on an lv_canvas
 *
 * The renderer is a pure function of (snapshot, ctx): pet_draw_scene() clears
 * the canvas and redraws the whole scene every frame. All animation is driven
 * by ctx->t_ms (advanced by the UI timer); only blinking and the particle
 * pool keep small state machines inside the context.
 *
 * Must be called from the UI thread (lv_canvas draw API limitation).
 */
#ifndef PET_DRAW_H
#define PET_DRAW_H

#include "lvgl.h"

#include "pet_core.h"

#define PET_CANVAS_W 160
#define PET_CANVAS_H 160

typedef enum {
    PET_P_NONE = 0,
    PET_P_RICE,  /* falling rice grains */
    PET_P_BUBBLE,/* rising soap bubbles */
    PET_P_HEART, /* rising hearts */
    PET_P_ZZZ,   /* rising "Z" text */
} pet_particle_kind_t;

typedef struct {
    uint8_t alive;
    uint8_t kind;
    int16_t x, y;
    int16_t vy;
    uint8_t life; /* frames remaining */
} pet_particle_t;

typedef struct {
    uint32_t t_ms;         /* advanced by the UI timer */
    uint32_t last_t_ms;    /* for particle integration */
    uint32_t blink_next_ms;
    uint8_t blink_closed;  /* 1 during the 120 ms blink */
    uint32_t evt;          /* current reaction event (pet_evt_t), 0 = none */
    uint32_t evt_until_ms;
    pet_particle_t part[8];
} pet_draw_ctx_t;

void pet_draw_ctx_reset(pet_draw_ctx_t *ctx);

/* Start a reaction animation (e.g. PET_EVT_EAT) lasting ~2 s. */
void pet_draw_play_event(pet_draw_ctx_t *ctx, uint32_t evt);

/* Redraw the whole scene. */
void pet_draw_scene(lv_obj_t *canvas, const pet_snapshot_t *snap, pet_draw_ctx_t *ctx);

#endif /* PET_DRAW_H */
