/*
 * vhw.h - software model of the X68000 video hardware.
 *
 * Implements the video half of the HAL (hal_pcg_load / hal_palette_set /
 * hal_bg_tile / hal_bg_scroll / hal_spr) by storing into an internal state that
 * mirrors the real chips, then rasterises that state into an RGBA framebuffer.
 * Platform backends call vhw_render_rgba() once per frame and present the result.
 *
 * This is the single source of truth for "what the X68000 would show", so the
 * SDL window and any headless/CI render are pixel-identical.
 */
#ifndef VHW_H
#define VHW_H

#include <stdint.h>
#include "hal.h"

/* Rasterise current video state into out[VHW_SCREEN_W * VHW_SCREEN_H] as
 * 0xAARRGGBB (matches SDL_PIXELFORMAT_ARGB8888). */
void vhw_render_rgba(uint32_t *out);

/* Budget diagnostics for the most recent rendered frame. */
int  vhw_last_sprite_count(void);   /* enabled sprites drawn (<= 128)        */
int  vhw_last_max_per_line(void);   /* peak sprites on one scanline (<= 32)  */

#endif /* VHW_H */
