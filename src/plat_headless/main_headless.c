/*
 * main_headless.c - SDL-free driver used for CI / parity verification.
 *
 * Runs the shared game loop with a scripted input sequence, prints an FNV-1a
 * hash of the framebuffer at key frames (the "state hash" used to check that
 * the PC and X68000 builds stay in lockstep), and optionally dumps frames as
 * PPM images (<prefix>_<frame>.ppm).
 *
 *   usage: x68game_headless [frames] [ppm_prefix]
 */
#include "game.h"
#include "vhw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_fb[VHW_SCREEN_W * VHW_SCREEN_H];
static int g_frame;
static int g_total;

/* Scripted pad: walk right the whole time, jump every ~50 frames (1-frame
 * pulse so the rising-edge jump fires once). Deterministic by frame. */
uint16_t hal_input(void)
{
    uint16_t pad = 0;
    if (g_frame >= 20) pad |= PAD_RIGHT;
    if (g_frame >= 40 && (g_frame % 50) == 0) pad |= PAD_A;
    return pad;
}

int hal_vsync(void)
{
    vhw_render_rgba(g_fb);
    g_frame++;
    return g_frame < g_total;
}

static uint32_t fnv1a(const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void write_ppm(const char *prefix, int frame)
{
    char path[256];
    snprintf(path, sizeof(path), "%s_%03d.ppm", prefix, frame);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", VHW_SCREEN_W, VHW_SCREEN_H);
    for (int i = 0; i < VHW_SCREEN_W * VHW_SCREEN_H; i++) {
        uint32_t c = g_fb[i];
        unsigned char rgb[3] = { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

int main(int argc, char **argv)
{
    g_total = (argc > 1) ? atoi(argv[1]) : 300;
    const char *prefix = (argc > 2) ? argv[2] : NULL;

    game_init();
    while (1) {
        uint16_t pad = hal_input();
        game_step(pad);
        int cont = hal_vsync();
        int key = (g_frame == 1 || g_frame == 100 || g_frame == 200 || !cont);
        if (key)
            printf("frame %4d  fbhash=%08x  spr=%d  max/line=%d\n",
                   g_frame, fnv1a(g_fb, sizeof(g_fb)),
                   vhw_last_sprite_count(), vhw_last_max_per_line());
        if (prefix && (g_frame == 90 || g_frame == 180 || g_frame == 270 || !cont))
            write_ppm(prefix, g_frame);
        if (!cont) break;
    }
    return 0;
}
