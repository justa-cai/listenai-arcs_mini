/*
 * pet_draw.c - procedural pet rendering on an lv_canvas
 *
 * Scene layout inside the 160x160 canvas (local coordinates):
 *   ground line y=138, body center (80, 92), egg center (80, 100).
 * The pet is drawn from layered AA primitives: shadow -> back features
 * (adult ears / baby hair tuft) -> body (two-tone fake gradient) -> belly ->
 * eyes/mouth/blush -> stage marks (egg cracks, collar) -> poops -> particles.
 * All motion is a pure function of ctx->t_ms (breathing, walking, hopping,
 * event reactions); blinking and particles are tiny state machines.
 */
#include <math.h>
#include <string.h>

#include "lisa_ui_fonts.h"

#include "pet_draw.h"

#define GROUND_Y 138
#define BODY_CX 80
#define BODY_CY 92
#define EGG_CY 102

#define SIN2PI(t_ms, period) sinf(6.2831853f * (float)((t_ms) % (period)) / (float)(period))

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static lv_color_t col(uint8_t r, uint8_t g, uint8_t b)
{
    lv_color_t c;
    c.full = rgb565(r, g, b);
    return c;
}

/* palette */
#define C_BODY rgb565(0xFF, 0xE0, 0x9E)     /* cream */
#define C_BODY_DARK rgb565(0xEF, 0xB9, 0x66)
#define C_BELLY rgb565(0xFF, 0xF4, 0xD9)
#define C_EYE rgb565(0x3A, 0x2E, 0x2A)
#define C_BLUSH rgb565(0xFF, 0xA5, 0xB9)
#define C_EGG rgb565(0xFD, 0xF3, 0xDE)
#define C_EGG_DARK rgb565(0xE8, 0xD9, 0xB8)
#define C_CRACK rgb565(0x8A, 0x7B, 0x5C)
#define C_POOP rgb565(0x8B, 0x6F, 0x47)
#define C_SHADOW rgb565(0x30, 0x28, 0x20)
#define C_HEART rgb565(0xFF, 0x7F, 0xA6)
#define C_COLLAR rgb565(0xFF, 0x6B, 0x6B)

/* ---------------- primitive helpers ---------------- */

static void disc8(lv_obj_t *cv, int cx, int cy, int r, uint16_t color, lv_opa_t opa)
{
    /* octagon polygon: exact AA'd fill without arc-width hacks */
    static const float k = 0.9239f; /* cos(22.5deg) */
    static const float s = 0.3827f; /* sin(22.5deg) */
    lv_point_t pts[8];
    float rr = (float)r;
    int coords[8][2] = {
        {(int)(k * rr), (int)(s * rr)}, {(int)(s * rr), (int)(k * rr)},
        {(int)(-s * rr), (int)(k * rr)}, {(int)(-k * rr), (int)(s * rr)},
        {(int)(-k * rr), (int)(-s * rr)}, {(int)(-s * rr), -(int)(k * rr)},
        {(int)(s * rr), -(int)(k * rr)}, {(int)(k * rr), -(int)(s * rr)},
    };
    for (int i = 0; i < 8; i++) {
        pts[i].x = (lv_coord_t)(cx + coords[i][0]);
        pts[i].y = (lv_coord_t)(cy + coords[i][1]);
    }
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color.full = color;
    dsc.bg_opa = opa;
    dsc.border_opa = LV_OPA_TRANSP;
    dsc.radius = r / 3; /* rounds the octagon into a soft blob */
    lv_canvas_draw_polygon(cv, pts, 8, &dsc);
}

static void stroke_circle(lv_obj_t *cv, int cx, int cy, int r, int width, uint16_t color,
                          int start, int end, lv_opa_t opa)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color.full = color;
    dsc.width = width;
    dsc.opa = opa;
    lv_canvas_draw_arc(cv, cx, cy, r, start, end, &dsc);
}

static void line(lv_obj_t *cv, int x1, int y1, int x2, int y2, int width, uint16_t color,
                 lv_opa_t opa)
{
    lv_point_t pts[2] = {{(lv_coord_t)x1, (lv_coord_t)y1}, {(lv_coord_t)x2, (lv_coord_t)y2}};
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color.full = color;
    dsc.width = width;
    dsc.opa = opa;
    dsc.round_start = 1;
    dsc.round_end = 1;
    lv_canvas_draw_line(cv, pts, 2, &dsc);
}

static void rect(lv_obj_t *cv, int x, int y, int w, int h, int radius, uint16_t color,
                 lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color.full = color;
    dsc.bg_opa = opa;
    dsc.radius = radius;
    lv_canvas_draw_rect(cv, x, y, w, h, &dsc);
}

/* ---------------- particles ---------------- */

static void spawn_particle(pet_draw_ctx_t *ctx, uint8_t kind, int x, int y, int vy)
{
    for (int i = 0; i < 8; i++) {
        if (!ctx->part[i].alive) {
            ctx->part[i].alive = 1;
            ctx->part[i].kind = kind;
            ctx->part[i].x = (int16_t)x;
            ctx->part[i].y = (int16_t)y;
            ctx->part[i].vy = (int16_t)vy;
            ctx->part[i].life = 50; /* ~1.6 s at 15 fps */
            return;
        }
    }
}

static void update_particles(pet_draw_ctx_t *ctx, uint32_t dt_ms)
{
    int steps = dt_ms > 200 ? 1 : (int)(dt_ms / 66) + 1; /* coarse integration */
    for (int i = 0; i < 8; i++) {
        pet_particle_t *p = &ctx->part[i];
        if (!p->alive) {
            continue;
        }
        for (int s = 0; s < steps; s++) {
            p->y += p->vy;
            if (p->kind == PET_P_RICE && p->y > GROUND_Y - 4) {
                p->alive = 0;
            }
            if (p->y < 6 || p->y > 158) {
                p->alive = 0;
                break;
            }
        }
        if (p->life <= (uint8_t)steps) {
            p->alive = 0;
        } else {
            p->life -= (uint8_t)steps;
        }
    }
}

static void draw_particles(lv_obj_t *cv, pet_draw_ctx_t *ctx)
{
    for (int i = 0; i < 8; i++) {
        pet_particle_t *p = &ctx->part[i];
        if (!p->alive) {
            continue;
        }
        lv_opa_t opa = p->life > 20 ? LV_OPA_70 : (lv_opa_t)(p->life * 3);
        switch (p->kind) {
        case PET_P_RICE:
            rect(cv, p->x, p->y, 4, 4, 2, rgb565(0xFF, 0xF7, 0xE0), opa);
            break;
        case PET_P_BUBBLE:
            stroke_circle(cv, p->x, p->y, 4, 1, rgb565(0xFF, 0xFF, 0xFF), 0, 360, opa);
            break;
        case PET_P_HEART:
            disc8(cv, p->x - 2, p->y, 2, C_HEART, opa);
            disc8(cv, p->x + 2, p->y, 2, C_HEART, opa);
            rect(cv, p->x - 3, p->y + 1, 7, 3, 1, C_HEART, opa);
            break;
        case PET_P_ZZZ: {
            lv_draw_label_dsc_t dsc;
            lv_draw_label_dsc_init(&dsc);
            dsc.color = col(0x8A, 0x9E, 0xC8);
            dsc.font = &lv_font_chinese_16;
            dsc.opa = opa;
            lv_canvas_draw_text(cv, p->x, p->y, 20, &dsc, "z");
            break;
        }
        default:
            break;
        }
    }
}

/* ---------------- scene ---------------- */

void pet_draw_ctx_reset(pet_draw_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->blink_next_ms = 2500;
}

void pet_draw_play_event(pet_draw_ctx_t *ctx, uint32_t evt)
{
    ctx->evt = evt;
    ctx->evt_until_ms = ctx->t_ms + 2000;

    switch (evt) {
    case PET_EVT_EAT:
        for (int i = 0; i < 3; i++) {
            spawn_particle(ctx, PET_P_RICE, BODY_CX - 8 + i * 8, BODY_CY - 20, 2);
        }
        break;
    case PET_EVT_CLEAN:
        for (int i = 0; i < 5; i++) {
            spawn_particle(ctx, PET_P_BUBBLE, BODY_CX - 24 + i * 12, BODY_CY + 10 - (i % 3) * 8,
                           -2);
        }
        break;
    case PET_EVT_PLAY:
    case PET_EVT_LOVE:
    case PET_EVT_PAT:
    case PET_EVT_GREET: {
        int n = evt == PET_EVT_LOVE ? 6 : 2;
        for (int i = 0; i < n; i++) {
            spawn_particle(ctx, PET_P_HEART, BODY_CX - 18 + i * 7, BODY_CY - 24 - (i % 2) * 6,
                           -1);
        }
        break;
    }
    default:
        break;
    }
}

static void draw_face(lv_obj_t *cv, const pet_snapshot_t *s, int cx, int cy, uint32_t t,
                      pet_draw_ctx_t *ctx, int eye_style)
{
    /* eye_style: 0 normal, 1 happy-closed (^ ^), 2 sleeping (down arcs) */
    bool blinking = ctx->blink_closed && eye_style == 0;
    int ex = 11, ey = -3;

    for (int side = -1; side <= 1; side += 2) {
        int x = cx + side * ex;
        int y = cy + ey;
        if (eye_style == 1) { /* happy ^^ */
            stroke_circle(cv, x, y + 2, 5, 2, C_EYE, 180, 360, LV_OPA_COVER);
        } else if (eye_style == 2 || blinking) { /* closed - */
            line(cv, x - 4, y, x + 4, y, 2, C_EYE, LV_OPA_COVER);
        } else { /* open: round eye + white highlight */
            disc8(cv, x, y, 4, C_EYE, LV_OPA_COVER);
            disc8(cv, x + 1, y - 1, 2, rgb565(0xFF, 0xFF, 0xFF), LV_OPA_80);
        }
    }

    /* mouth */
    int my = cy + 7;
    if (s->mood == PET_MOOD_SICK) {
        /* wavy sick mouth */
        line(cv, cx - 5, my, cx - 2, my + 2, 1, C_EYE, LV_OPA_COVER);
        line(cv, cx - 2, my + 2, cx + 2, my - 1, 1, C_EYE, LV_OPA_COVER);
        line(cv, cx + 2, my - 1, cx + 5, my + 1, 1, C_EYE, LV_OPA_COVER);
    } else if (ctx->evt == PET_EVT_EAT && t < ctx->evt_until_ms) {
        /* chomping: alternating open/closed every 150 ms */
        if (((t / 150) % 2) == 0) {
            disc8(cv, cx, my + 1, 3, rgb565(0x8C, 0x4A, 0x4A), LV_OPA_COVER);
        } else {
            line(cv, cx - 3, my, cx + 3, my, 2, C_EYE, LV_OPA_COVER);
        }
    } else if (s->mood == PET_MOOD_HUNGRY || s->mood == PET_MOOD_SAD) {
        stroke_circle(cv, cx, my + 3, 4, 2, C_EYE, 20, 160, LV_OPA_COVER); /* frown */
    } else {
        stroke_circle(cv, cx, my - 1, 4, 2, C_EYE, 200, 340, LV_OPA_COVER); /* smile */
    }

    /* blush: always on (cuter), stronger for content/adult */
    lv_opa_t blush_opa = (s->mood == PET_MOOD_CONTENT || s->stage == PET_STAGE_ADULT)
                             ? LV_OPA_50
                             : LV_OPA_30;
    stroke_circle(cv, cx - 16, cy + 3, 3, 2, C_BLUSH, 0, 360, blush_opa);
    stroke_circle(cv, cx + 16, cy + 3, 3, 2, C_BLUSH, 0, 360, blush_opa);
}

/* Fill an egg shape by stacking discs along a vertical egg profile: radius
 * follows an ellipse, narrowed toward the top. outline_disc adds a rim. */
static void egg_stack(lv_obj_t *cv, int cx, int cy, int rx, int ry, int rim, uint16_t color,
                      lv_opa_t opa)
{
    const int n = 9;
    for (int k = 0; k < n; k++) {
        float yy = -1.0f + 2.0f * (float)k / (float)(n - 1); /* -1 (top) .. 1 (bottom) */
        float narrow = yy < 0 ? (1.0f + 0.22f * yy) : 1.0f;  /* top 22% narrower */
        int r = (int)(rx * sqrtf(1.0f - yy * yy) * narrow);
        if (r < 1) {
            continue;
        }
        disc8(cv, cx, cy + (int)(yy * ry), r + rim, color, opa);
    }
}

static void draw_egg(lv_obj_t *cv, const pet_snapshot_t *s, uint32_t t, pet_draw_ctx_t *ctx)
{
    int cx = BODY_CX;
    int cy = EGG_CY;
    const int rx = 32;
    const int ry = 36;

    /* wobble when close to hatching */
    if (s->mood == PET_MOOD_EGG_WOBBLE) {
        cx += (int)(3 * SIN2PI(t, 600));
    }
    if (ctx->evt == PET_EVT_HATCH && t < ctx->evt_until_ms) {
        cx += (((t / 80) % 2) == 0) ? 3 : -3;
    }

    /* nest: soft brown mound under the egg */
    disc8(cv, cx, GROUND_Y - 2, 42, rgb565(0xC8, 0xA8, 0x82), LV_OPA_COVER);
    disc8(cv, cx, GROUND_Y - 6, 38, rgb565(0xB2, 0x8E, 0x68), LV_OPA_COVER);

    /* egg body: dark rim + shell + upper-left shine + freckles */
    egg_stack(cv, cx, cy + 1, rx, ry, 2, C_EGG_DARK, LV_OPA_COVER);
    egg_stack(cv, cx, cy - 1, rx - 2, ry - 2, 0, C_EGG, LV_OPA_COVER);
    disc8(cv, cx - rx / 2, cy - ry / 3, 6, rgb565(0xFF, 0xFF, 0xF6), LV_OPA_60);
    disc8(cv, cx + rx / 3, cy + ry / 4, 3, C_EGG_DARK, LV_OPA_30);
    disc8(cv, cx - rx / 4, cy + ry / 3, 2, C_EGG_DARK, LV_OPA_30);

    /* cracks: a zigzag across the shell, growing with egg_crack */
    if (s->egg_crack >= 1) {
        int yy = cy - 6;
        line(cv, cx - 14, yy, cx - 7, yy + 5, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx - 7, yy + 5, cx - 12, yy + 10, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx - 12, yy + 10, cx - 3, yy + 14, 2, C_CRACK, LV_OPA_COVER);
    }
    if (s->egg_crack >= 2) {
        int yy = cy - 14;
        line(cv, cx + 4, yy, cx + 10, yy + 6, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx + 10, yy + 6, cx + 3, yy + 10, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx + 3, yy + 10, cx + 11, yy + 15, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx + 11, yy + 15, cx + 5, yy + 20, 2, C_CRACK, LV_OPA_COVER);
        line(cv, cx - 5, cy + 4, cx + 6, cy + 8, 2, C_CRACK, LV_OPA_COVER);
    }

    /* sleepy eyes on the shell */
    line(cv, cx - 8, cy + 12, cx - 3, cy + 12, 2, C_EYE, LV_OPA_50);
    line(cv, cx + 3, cy + 12, cx + 8, cy + 12, 2, C_EYE, LV_OPA_50);
}

void pet_draw_scene(lv_obj_t *canvas, const pet_snapshot_t *snap, pet_draw_ctx_t *ctx)
{
    uint32_t t = ctx->t_ms;
    uint32_t dt = t - ctx->last_t_ms;
    if (dt > 500) {
        dt = 500;
    }
    ctx->last_t_ms = t;

    /* blink state machine */
    if (ctx->blink_closed) {
        if (t > ctx->blink_next_ms) {
            ctx->blink_closed = 0;
            ctx->blink_next_ms = t + 2000 + (t % 2400);
        }
    } else if (t > ctx->blink_next_ms) {
        ctx->blink_closed = 1;
        ctx->blink_next_ms = t + 120;
    }

    update_particles(ctx, dt);

    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    /* ground */
    rect(canvas, 0, GROUND_Y, PET_CANVAS_W, 3, 1, rgb565(0x6B, 0x8E, 0x5A), LV_OPA_60);
    rect(canvas, 16, GROUND_Y + 3, PET_CANVAS_W - 32, 2, 1, rgb565(0x6B, 0x8E, 0x5A), LV_OPA_30);

    if (snap->stage == PET_STAGE_EGG) {
        disc8(canvas, BODY_CX, GROUND_Y - 8, 26, C_SHADOW, LV_OPA_20);
        draw_egg(canvas, snap, t, ctx);
        draw_particles(canvas, ctx);
        return;
    }

    /* ---- body motion ---- */
    bool sleeping = snap->sleeping || snap->mood == PET_MOOD_SLEEP;
    bool reacting = ctx->evt != 0 && t < ctx->evt_until_ms;

    int r = (snap->stage == PET_STAGE_ADULT) ? 36 : 28;
    int cx = BODY_CX;
    int cy = BODY_CY;

    /* breathing (slower when asleep) */
    float breathe = sleeping ? (0.6f * SIN2PI(t, 6000)) : SIN2PI(t, 3000);
    r += (int)(2 * breathe);
    cy -= (breathe > 0.0f) ? 1 : 0; /* subtle lift */

    /* idle walk (awake, not reacting) */
    if (!sleeping && !reacting && (snap->mood == PET_MOOD_NORMAL || snap->mood == PET_MOOD_CONTENT)) {
        cx += (int)(36 * SIN2PI(t, 9000));
        cy -= (int)(2 * fabsf(SIN2PI(t, 300))); /* trot bounce */
    }

    /* happy hop */
    if (!sleeping && snap->mood == PET_MOOD_CONTENT && !reacting) {
        cy -= (int)(8 * fabsf(SIN2PI(t, 750)));
    }

    /* reaction overlays */
    if (reacting) {
        switch (ctx->evt) {
        case PET_EVT_EAT:
            cx += 4; /* lean toward the food */
            break;
        case PET_EVT_CLEAN:
            cx += (int)(3 * SIN2PI(t, 300));
            break;
        case PET_EVT_PLAY:
        case PET_EVT_GREET:
        case PET_EVT_WOKE:
            cy -= (int)(12 * fabsf(SIN2PI(t, 375))); /* bigger triple-ish hop */
            break;
        case PET_EVT_REJECT:
            cx += (((t / 80) % 2) == 0) ? 3 : -3;
            break;
        case PET_EVT_EVOLVE:
            /* flash disc behind the body, shrinking */
            stroke_circle(canvas, BODY_CX, BODY_CY, 50 - (int)((t % 400) / 400.0f * 20),
                          50 - (int)((t % 400) / 400.0f * 20) > 0 ? 30 : 1,
                          rgb565(0xFF, 0xFF, 0xE0), 0, 360, LV_OPA_30);
            break;
        default:
            break;
        }
    }

    /* sleeping zzz particles */
    if (sleeping && (t % 1200) < 66) {
        spawn_particle(ctx, PET_P_ZZZ, cx + 24, cy - 30, -1);
    }

    /* ---- draw body layers ---- */
    disc8(canvas, cx, GROUND_Y - 6, r - 4, C_SHADOW, LV_OPA_20); /* ground shadow */

    /* back features */
    if (snap->stage == PET_STAGE_ADULT) {
        /* round ears behind the body with pink inner ear */
        int ear_dx = r - 6;
        int ear_dy = cy - r + 8;
        for (int side = -1; side <= 1; side += 2) {
            disc8(canvas, cx + side * ear_dx, ear_dy, 11, C_BODY_DARK, LV_OPA_COVER);
            disc8(canvas, cx + side * ear_dx, ear_dy - 1, 9, C_BODY, LV_OPA_COVER);
            disc8(canvas, cx + side * ear_dx, ear_dy, 4, C_BLUSH, LV_OPA_70);
        }
    } else {
        /* baby hair tufts: two little leaves on top */
        line(canvas, cx - 2, cy - r + 3, cx - 5, cy - r - 6, 2, C_BODY_DARK, LV_OPA_COVER);
        line(canvas, cx + 2, cy - r + 3, cx + 5, cy - r - 6, 2, C_BODY_DARK, LV_OPA_COVER);
        disc8(canvas, cx - 5, cy - r - 7, 2, C_BODY_DARK, LV_OPA_COVER);
        disc8(canvas, cx + 5, cy - r - 7, 2, C_BODY_DARK, LV_OPA_COVER);
    }

    /* body: two-tone fake gradient */
    disc8(canvas, cx, cy + 3, r, C_BODY_DARK, LV_OPA_COVER);
    disc8(canvas, cx, cy - 1, r - 2, C_BODY, LV_OPA_COVER);
    /* belly */
    disc8(canvas, cx, cy + r / 3, r - 10, C_BELLY, LV_OPA_COVER);

    /* collar for adults */
    if (snap->stage == PET_STAGE_ADULT) {
        stroke_circle(canvas, cx, cy + r / 2, r - 4, 3, C_COLLAR, 30, 150, LV_OPA_COVER);
    }

    /* face */
    int eye_style = 0;
    if (sleeping) {
        eye_style = 2;
    } else if (snap->mood == PET_MOOD_CONTENT || ctx->evt == PET_EVT_PAT ||
               ctx->evt == PET_EVT_LOVE) {
        eye_style = 1;
    }
    if (snap->sick && !sleeping) {
        eye_style = 0; /* tired normal eyes, sick mouth shows it */
    }
    draw_face(canvas, snap, cx, cy - 2, t, ctx, eye_style);

    /* poops next to the pet */
    for (int i = 0; i < snap->poops && i < 3; i++) {
        int px = 24 + i * 14;
        int py = GROUND_Y - 4;
        disc8(canvas, px, py, 5, C_POOP, LV_OPA_COVER);
        disc8(canvas, px, py - 6, 4, C_POOP, LV_OPA_COVER);
        disc8(canvas, px, py - 11, 3, C_POOP, LV_OPA_COVER);
    }

    draw_particles(canvas, ctx);
}
