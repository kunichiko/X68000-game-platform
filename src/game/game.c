/*
 * game.c - minimal vertical slice: a player sprite that walks left/right on a
 * tiled ground. Pure shared logic; talks only to the HAL.
 */
#include "game.h"
#include "hal.h"

/* --- PCG tile ids --- */
#define TILE_BRICK   0
#define TILE_PLAYER  1

/* --- palette blocks --- */
#define PAL_BG       0
#define PAL_PLAYER   1

/* --- ground line (in tiles) on the 16-tile-tall screen --- */
#define GROUND_TY    13

/* Pack a 16x16 tile described as 16 strings of hex nibbles ('.' = 0) into the
 * 128-byte 4bpp PCG format and upload it. */
static void load_tile(int id, const char *rows[16])
{
    uint8_t pat[128];
    for (int y = 0; y < 16; y++) {
        const char *r = rows[y];
        for (int x = 0; x < 8; x++) {
            int hi = r[x * 2],     lo = r[x * 2 + 1];
            hi = (hi == '.') ? 0 : (hi <= '9' ? hi - '0' : (hi | 0x20) - 'a' + 10);
            lo = (lo == '.') ? 0 : (lo <= '9' ? lo - '0' : (lo | 0x20) - 'a' + 10);
            pat[y * 8 + x] = (uint8_t)((hi << 4) | lo);
        }
    }
    hal_pcg_load(id, pat);
}

static const char *BRICK[16] = {
    "3333333333333333",
    "3111111131111111",
    "3111111131111111",
    "3111111131111111",
    "3111111131111111",
    "3111111131111111",
    "3111111131111111",
    "3111111131111111",
    "3333333333333333",
    "3111311111113111",
    "3111311111113111",
    "3111311111113111",
    "3111311111113111",
    "3111311111113111",
    "3111311111113111",
    "3111311111113111",
};

static const char *PLAYER[16] = {
    ".....33333......",
    "....3222223.....",
    "....3222223.....",
    "....3232323.....",
    "....3222223.....",
    ".....32223......",
    "....33333333....",
    "...3111111113...",
    "..31111111113...",
    "..31111111113...",
    "..31111111113...",
    "...3111111133...",
    "....311.113.....",
    "....311.113.....",
    "....331.133.....",
    "................",
};

/* --- player state (8.8 fixed-point) --- */
static fix8 s_px, s_py;
static const fix8 WALK_SPEED = FIX8(2);   /* px per tick */

void game_init(void)
{
    /* Palette. block 0 = BG, block 1 = player. Index 0 of every block is
     * transparent; palette[0] doubles as the screen backdrop (sky). */
    hal_palette_set(0, GRB(12, 18, 28));                 /* sky backdrop      */
    hal_palette_set(PAL_BG * 16 + 1, GRB(24, 10, 6));    /* brick             */
    hal_palette_set(PAL_BG * 16 + 3, GRB(14, 6, 4));     /* mortar / outline  */

    hal_palette_set(PAL_PLAYER * 16 + 1, GRB(28, 6, 6)); /* shirt             */
    hal_palette_set(PAL_PLAYER * 16 + 2, GRB(31, 22, 16));/* skin             */
    hal_palette_set(PAL_PLAYER * 16 + 3, GRB(4, 3, 3));  /* outline           */

    load_tile(TILE_BRICK, BRICK);
    load_tile(TILE_PLAYER, PLAYER);

    /* Fill the ground: rows GROUND_TY..bottom of the visible screen. */
    for (int ty = GROUND_TY; ty < 16; ty++)
        for (int tx = 0; tx < VHW_BG_W; tx++)
            hal_bg_tile(tx, ty, ATTR(TILE_BRICK, PAL_BG, 0, 0));

    hal_bg_scroll(0, 0);

    /* Player stands with its feet on the ground line. */
    s_px = FIX8(120);
    s_py = FIX8(GROUND_TY * 16 - 16);
}

void game_step(uint16_t pad)
{
    int hflip = 0;
    if (pad & PAD_LEFT)  { s_px -= WALK_SPEED; hflip = 1; }
    if (pad & PAD_RIGHT) { s_px += WALK_SPEED; hflip = 0; }

    /* Clamp to the visible screen. */
    if (s_px < 0)                       s_px = 0;
    if (s_px > FIX8(VHW_SCREEN_W - 16)) s_px = FIX8(VHW_SCREEN_W - 16);

    hal_spr(0, s_px, s_py, ATTR(TILE_PLAYER, PAL_PLAYER, hflip, 0), 1);
}
