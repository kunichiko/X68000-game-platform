/*
 * game.c - platformer vertical slice: gravity, jump, tilemap collision, and a
 * camera that scrolls a level wider than the screen. Pure shared logic; talks
 * only to the HAL. 8.8 fixed-point, no float (all fractional constants are
 * built from integer arithmetic on FIX8_ONE).
 */
#include "game.h"
#include "hal.h"

/* --- PCG tile ids --- */
#define TILE_BRICK   0
#define TILE_PLAYER  1

/* --- palette blocks --- */
#define PAL_BG       0
#define PAL_PLAYER   1

/* --- level (tiles). 16 rows tall (= screen), LEVEL_W wide (scrolls). --- */
#define LEVEL_W      64
#define LEVEL_H      16
static uint8_t s_solid[LEVEL_H][LEVEL_W];

/* --- player hitbox (px) and physics (8.8 fixed-point) --- */
#define HB_W        12
#define HB_H        16
#define SPR_OFF_X   (-2)                 /* draw 16px art centred over 12px box */

#define GRAVITY     (FIX8_ONE * 3 / 10)  /* 0.30 px/tick^2 */
#define MAX_FALL    FIX8(6)
#define WALK_ACCEL  (FIX8_ONE / 2)       /* 0.50 */
#define WALK_MAX    FIX8(2)
#define FRICTION    (FIX8_ONE / 2)
#define JUMP_VY     (-FIX8(5))

static fix8     s_px, s_py, s_vx, s_vy;
static int      s_on_ground, s_face_left;
static uint16_t s_prev_pad;

/* --- sound state --- */
#define STEP_PERIOD 14                   /* ticks between footsteps while walking */
static int s_prev_on_ground, s_step_timer;

/* Pack a 16x16 tile (16 strings of hex nibbles, '.' = 0) into 4bpp and upload. */
static void load_tile(int id, const char *rows[16])
{
    uint8_t pat[128];
    for (int y = 0; y < 16; y++) {
        const char *r = rows[y];
        for (int x = 0; x < 8; x++) {
            int hi = r[x * 2], lo = r[x * 2 + 1];
            hi = (hi == '.') ? 0 : (hi <= '9' ? hi - '0' : (hi | 0x20) - 'a' + 10);
            lo = (lo == '.') ? 0 : (lo <= '9' ? lo - '0' : (lo | 0x20) - 'a' + 10);
            pat[y * 8 + x] = (uint8_t)((hi << 4) | lo);
        }
    }
    hal_pcg_load(id, pat);
}

static const char *BRICK[16] = {
    "3333333333333333",
    "3111111131111111", "3111111131111111", "3111111131111111",
    "3111111131111111", "3111111131111111", "3111111131111111",
    "3111111131111111",
    "3333333333333333",
    "3111311111113111", "3111311111113111", "3111311111113111",
    "3111311111113111", "3111311111113111", "3111311111113111",
    "3111311111113111",
};

static const char *PLAYER[16] = {
    ".....33333......",
    "....3222223.....", "....3222223.....", "....3232323.....",
    "....3222223.....", ".....32223......",
    "....33333333....",
    "...3111111113...", "..31111111113...", "..31111111113...",
    "..31111111113...", "...3111111133...",
    "....311.113.....", "....311.113.....", "....331.133.....",
    "................",
};

/* Place a solid brick tile in both the collision map and the BG tilemap. */
static void set_brick(int tx, int ty)
{
    if ((unsigned)tx >= LEVEL_W || (unsigned)ty >= LEVEL_H) return;
    s_solid[ty][tx] = 1;
    hal_bg_tile(tx, ty, ATTR(TILE_BRICK, PAL_BG, 0, 0));
}

static int solid_at(int px, int py)
{
    int tx = px >> 4, ty = py >> 4;
    if (tx < 0 || tx >= LEVEL_W) return 1;   /* level walls */
    if (py < 0)                  return 0;    /* open sky above */
    if (ty >= LEVEL_H)           return 1;    /* floor below the world */
    return s_solid[ty][tx];
}

static void build_level(void)
{
    /* ground: bottom three rows */
    for (int tx = 0; tx < LEVEL_W; tx++) {
        set_brick(tx, 13); set_brick(tx, 14); set_brick(tx, 15);
    }
    /* floating platforms */
    for (int tx = 8;  tx <= 12; tx++) set_brick(tx, 10);
    for (int tx = 18; tx <= 22; tx++) set_brick(tx, 8);
    for (int tx = 28; tx <= 30; tx++) set_brick(tx, 6);
    for (int tx = 40; tx <= 48; tx++) set_brick(tx, 9);
    /* a wall to bump into / wall-adjacent jumping */
    for (int ty = 9; ty <= 12; ty++) set_brick(55, ty);
}

void game_init(void)
{
    hal_palette_set(0, GRB(12, 18, 28));                  /* sky backdrop     */
    hal_palette_set(PAL_BG * 16 + 1, GRB(24, 10, 6));     /* brick            */
    hal_palette_set(PAL_BG * 16 + 3, GRB(14, 6, 4));      /* mortar / outline */
    hal_palette_set(PAL_PLAYER * 16 + 1, GRB(28, 6, 6));  /* shirt            */
    hal_palette_set(PAL_PLAYER * 16 + 2, GRB(31, 22, 16));/* skin             */
    hal_palette_set(PAL_PLAYER * 16 + 3, GRB(4, 3, 3));   /* outline          */

    load_tile(TILE_BRICK, BRICK);
    load_tile(TILE_PLAYER, PLAYER);
    build_level();

    s_px = FIX8(32);  s_py = FIX8(12 * 16);
    s_vx = s_vy = 0;
    s_on_ground = 0; s_face_left = 0; s_prev_pad = 0;
    s_prev_on_ground = 1; s_step_timer = 0;
}

/* Move on X, then resolve against the tilemap. */
static void move_x(void)
{
    s_px += s_vx;
    int ix = FIX8_TO_INT(s_px), iy = FIX8_TO_INT(s_py);
    int top = iy, bot = iy + HB_H - 1;
    if (s_vx > 0) {
        int r = ix + HB_W - 1;
        if (solid_at(r, top) || solid_at(r, bot)) {
            s_px = FIX8_FROM_INT((r >> 4) * 16 - HB_W);
            s_vx = 0;
        }
    } else if (s_vx < 0) {
        if (solid_at(ix, top) || solid_at(ix, bot)) {
            s_px = FIX8_FROM_INT(((ix >> 4) + 1) * 16);
            s_vx = 0;
        }
    }
}

/* Move on Y, then resolve; sets s_on_ground on a downward landing. */
static void move_y(void)
{
    s_py += s_vy;
    int ix = FIX8_TO_INT(s_px), iy = FIX8_TO_INT(s_py);
    int lft = ix, rgt = ix + HB_W - 1;
    s_on_ground = 0;
    if (s_vy > 0) {
        int b = iy + HB_H - 1;
        if (solid_at(lft, b) || solid_at(rgt, b)) {
            s_py = FIX8_FROM_INT((b >> 4) * 16 - HB_H);
            s_vy = 0; s_on_ground = 1;
        }
    } else if (s_vy < 0) {
        if (solid_at(lft, iy) || solid_at(rgt, iy)) {
            s_py = FIX8_FROM_INT(((iy >> 4) + 1) * 16);
            s_vy = 0;
        }
    }
}

void game_step(uint16_t pad)
{
    uint16_t pressed = (uint16_t)(pad & ~s_prev_pad);   /* rising edge */

    /* --- horizontal: accelerate with input, friction otherwise --- */
    if (pad & PAD_LEFT)  { s_vx -= WALK_ACCEL; s_face_left = 1; }
    else if (pad & PAD_RIGHT) { s_vx += WALK_ACCEL; s_face_left = 0; }
    else {
        if (s_vx > FRICTION)       s_vx -= FRICTION;
        else if (s_vx < -FRICTION) s_vx += FRICTION;
        else                       s_vx = 0;
    }
    if (s_vx >  WALK_MAX) s_vx =  WALK_MAX;
    if (s_vx < -WALK_MAX) s_vx = -WALK_MAX;

    /* --- jump / gravity --- */
    if (s_on_ground && (pressed & (PAD_A | PAD_UP))) {
        s_vy = JUMP_VY;
        hal_ym_key(0, 72, INST_JUMP);
    }
    s_vy += GRAVITY;
    if (s_vy > MAX_FALL) s_vy = MAX_FALL;

    move_x();
    move_y();

    /* --- sound: footsteps while walking, thud on landing --- */
    if (s_on_ground && (s_vx > FRICTION || s_vx < -FRICTION)) {
        if (--s_step_timer <= 0) { hal_adpcm(SFX_STEP); s_step_timer = STEP_PERIOD; }
    } else {
        s_step_timer = 0;
    }
    if (s_on_ground && !s_prev_on_ground) hal_adpcm(SFX_LAND);
    s_prev_on_ground = s_on_ground;

    /* --- camera: centre on player, clamp to the level --- */
    int cam = FIX8_TO_INT(s_px) + HB_W / 2 - VHW_SCREEN_W / 2;
    if (cam < 0) cam = 0;
    if (cam > LEVEL_W * 16 - VHW_SCREEN_W) cam = LEVEL_W * 16 - VHW_SCREEN_W;
    hal_bg_scroll(FIX8_FROM_INT(cam), 0);

    /* --- draw player at screen coords --- */
    fix8 sx = s_px - FIX8_FROM_INT(cam) + FIX8(SPR_OFF_X);
    hal_spr(0, sx, s_py, ATTR(TILE_PLAYER, PAL_PLAYER, s_face_left, 0), 1);

    s_prev_pad = pad;
}
