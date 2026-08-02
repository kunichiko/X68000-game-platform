/*
 * hal.h - Hardware Abstraction Layer for the X68000 game platform.
 *
 * This is the ONLY interface the shared game logic (src/game) is allowed to
 * call. Every function maps directly onto an X68000 hardware primitive:
 *
 *   - PCG   : 16x16, 4bpp programmable character generator patterns, shared by
 *             both the BG tilemap and the hardware sprites (as on real HW).
 *   - BG    : one scrollable tilemap layer built from PCG tiles.
 *   - SPR   : up to 128 hardware sprites, also PCG patterns.
 *   - PAL   : 256 colour entries = 16 blocks x 16 colours, X68000 GRB format.
 *
 * The PC/Mac backend (src/plat_sdl, src/plat_headless) implements the video
 * calls via the software virtual hardware in src/vhw, and enforces the real
 * hardware budgets. The X68000 backend (future src/plat_x68) implements the
 * same calls with IOCS/DOS calls against the real chips.
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include "fixed.h"

/* --- virtual hardware dimensions (base X68000 game profile) --- */
#define VHW_SCREEN_W     256
#define VHW_SCREEN_H     256
#define VHW_MAX_SPRITES  128        /* hardware sprite table size            */
#define VHW_SPR_PER_LINE 32         /* raster limit; backend warns on exceed */
#define VHW_MAX_PCG      256        /* number of 16x16 patterns              */
#define VHW_BG_W         64         /* tilemap width  in tiles (1024px)      */
#define VHW_BG_H         64         /* tilemap height in tiles (1024px)      */
#define VHW_PAL_ENTRIES  256        /* 16 blocks x 16 colours                */

/* --- tile / sprite attribute word (shared encoding) --- */
#define ATTR(tile, pal, hf, vf) \
    ((uint16_t)(((tile) & 0x3FF) | ((hf) ? 0x400 : 0) | ((vf) ? 0x800 : 0) | \
                (((pal) & 0xF) << 12)))
#define ATTR_TILE(a)   ((a) & 0x3FF)
#define ATTR_HFLIP(a)  (((a) >> 10) & 1)
#define ATTR_VFLIP(a)  (((a) >> 11) & 1)
#define ATTR_PAL(a)    (((a) >> 12) & 0xF)

/* --- X68000 GRB colour helper: 5 bits each, r/g/b in 0..31 --- */
#define GRB(r, g, b) \
    ((uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) | (((b) & 0x1F) << 1)))

/* --- input bitmask --- */
#define PAD_UP     0x0001
#define PAD_DOWN   0x0002
#define PAD_LEFT   0x0004
#define PAD_RIGHT  0x0008
#define PAD_A      0x0010
#define PAD_B      0x0020
#define PAD_START  0x0040

/* --- video (platform-independent, implemented in src/vhw) --- */
void hal_pcg_load(int tile, const uint8_t *pattern);   /* 128 bytes, 4bpp, hi-nibble = left px */
void hal_palette_set(int index, uint16_t grb);         /* 0..255 */
void hal_bg_tile(int tx, int ty, uint16_t attr);
void hal_bg_scroll(fix8 x, fix8 y);
void hal_spr(int i, fix8 x, fix8 y, uint16_t attr, int enable);

/* --- platform I/O (implemented per backend) --- */
uint16_t hal_input(void);   /* current pad state */
int      hal_vsync(void);   /* present frame, sync to ~60Hz; 0 = quit requested */

/* --- audio --- */
/* Instrument / SFX ids: shared vocabulary between game logic and the backend
 * synth (PC) or the real YM2151/ADPCM chips (X68000). */
#define INST_JUMP  0        /* bright FM blip */

#define SFX_STEP   0        /* short footstep click (ADPCM) */
#define SFX_LAND   1        /* landing thud (ADPCM) */

void hal_ym_key(int ch, int note, int inst);  /* key-on FM note */
void hal_ym_off(int ch);                       /* key-off */
void hal_adpcm(int id);                         /* one-shot sampled SFX */

#endif /* HAL_H */
