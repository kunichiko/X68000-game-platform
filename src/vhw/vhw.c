/*
 * vhw.c - software X68000 video hardware. See vhw.h.
 */
#include "vhw.h"
#include <string.h>

/* --- state mirroring the real chips --- */
static uint8_t  s_pcg[VHW_MAX_PCG][128];        /* 4bpp patterns, 128B each */
static uint16_t s_pal_grb[VHW_PAL_ENTRIES];     /* X68000 GRB colour words  */
static uint32_t s_pal_rgb[VHW_PAL_ENTRIES];     /* cached 0xAARRGGBB        */
static uint16_t s_bg[VHW_BG_H * VHW_BG_W];      /* tilemap attribute words  */
static fix8     s_scroll_x, s_scroll_y;

typedef struct { fix8 x, y; uint16_t attr; uint8_t enable; } sprite_t;
static sprite_t s_spr[VHW_MAX_SPRITES];

static int s_last_spr_count;
static int s_last_max_per_line;

/* --- X68000 GRB (5:5:5 + intensity in bit0, shared LSB) -> 0xAARRGGBB --- */
static uint32_t grb_to_argb(uint16_t c)
{
    int g = (c >> 11) & 0x1F;
    int r = (c >> 6)  & 0x1F;
    int b = (c >> 1)  & 0x1F;
    int i = c & 1;
    int r6 = (r << 1) | i, g6 = (g << 1) | i, b6 = (b << 1) | i;
    int R = (r6 * 255) / 63, G = (g6 * 255) / 63, B = (b6 * 255) / 63;
    return 0xFF000000u | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
}

/* --- HAL video implementation --- */
void hal_pcg_load(int tile, const uint8_t *pattern)
{
    if (tile < 0 || tile >= VHW_MAX_PCG) return;
    memcpy(s_pcg[tile], pattern, 128);
}

void hal_palette_set(int index, uint16_t grb)
{
    if (index < 0 || index >= VHW_PAL_ENTRIES) return;
    s_pal_grb[index] = grb;
    s_pal_rgb[index] = grb_to_argb(grb);
}

void hal_bg_tile(int tx, int ty, uint16_t attr)
{
    if ((unsigned)tx >= VHW_BG_W || (unsigned)ty >= VHW_BG_H) return;
    s_bg[ty * VHW_BG_W + tx] = attr;
}

void hal_bg_scroll(fix8 x, fix8 y) { s_scroll_x = x; s_scroll_y = y; }

void hal_spr(int i, fix8 x, fix8 y, uint16_t attr, int enable)
{
    if ((unsigned)i >= VHW_MAX_SPRITES) return;
    s_spr[i].x = x; s_spr[i].y = y; s_spr[i].attr = attr;
    s_spr[i].enable = (uint8_t)(enable != 0);
}

/* pixel nibble from a PCG pattern, honouring flip */
static int pcg_pixel(int tile, int px, int py, int hf, int vf)
{
    const uint8_t *pat = s_pcg[tile];
    if (hf) px = 15 - px;
    if (vf) py = 15 - py;
    uint8_t byte = pat[py * 8 + (px >> 1)];
    return (px & 1) ? (byte & 0x0F) : (byte >> 4);
}

void vhw_render_rgba(uint32_t *out)
{
    uint32_t backdrop = s_pal_rgb[0];
    int sx0 = FIX8_TO_INT(s_scroll_x);
    int sy0 = FIX8_TO_INT(s_scroll_y);
    int x, y, i;

    /* --- backdrop + BG tilemap --- */
    for (y = 0; y < VHW_SCREEN_H; y++) {
        int wy = y + sy0;
        int ty = (wy >> 4) & (VHW_BG_H - 1);
        int py = wy & 15;
        uint32_t *row = out + y * VHW_SCREEN_W;
        for (x = 0; x < VHW_SCREEN_W; x++) {
            int wx = x + sx0;
            int tx = (wx >> 4) & (VHW_BG_W - 1);
            int px = wx & 15;
            uint16_t a = s_bg[ty * VHW_BG_W + tx];
            int n = pcg_pixel(ATTR_TILE(a), px, py, ATTR_HFLIP(a), ATTR_VFLIP(a));
            row[x] = n ? s_pal_rgb[ATTR_PAL(a) * 16 + n] : backdrop;
        }
    }

    /* --- sprites: draw 127..0 so sprite 0 ends up on top (X68000 priority) --- */
    {
        static uint8_t per_line[VHW_SCREEN_H];
        int count = 0;
        memset(per_line, 0, sizeof(per_line));
        for (i = VHW_MAX_SPRITES - 1; i >= 0; i--) {
            if (!s_spr[i].enable) continue;
            count++;
            int ox = FIX8_TO_INT(s_spr[i].x);
            int oy = FIX8_TO_INT(s_spr[i].y);
            uint16_t a = s_spr[i].attr;
            int tile = ATTR_TILE(a), pal = ATTR_PAL(a);
            int hf = ATTR_HFLIP(a), vf = ATTR_VFLIP(a);
            for (int py = 0; py < 16; py++) {
                int sy = oy + py;
                if ((unsigned)sy >= VHW_SCREEN_H) continue;
                if (per_line[sy] < 255) per_line[sy]++;
                uint32_t *row = out + sy * VHW_SCREEN_W;
                for (int px = 0; px < 16; px++) {
                    int sx = ox + px;
                    if ((unsigned)sx >= VHW_SCREEN_W) continue;
                    int n = pcg_pixel(tile, px, py, hf, vf);
                    if (n) row[sx] = s_pal_rgb[pal * 16 + n];
                }
            }
        }
        s_last_spr_count = count;
        s_last_max_per_line = 0;
        for (y = 0; y < VHW_SCREEN_H; y++)
            if (per_line[y] > s_last_max_per_line) s_last_max_per_line = per_line[y];
    }
}

int vhw_last_sprite_count(void)  { return s_last_spr_count; }
int vhw_last_max_per_line(void)  { return s_last_max_per_line; }
