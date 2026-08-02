/*
 * main_headless.c - SDL-free driver used for CI / parity verification.
 *
 * Runs the shared game loop with a scripted input sequence, prints an FNV-1a
 * hash of the framebuffer and the audio-event digest at key frames (the
 * "state hashes" used to check the PC and X68000 builds stay in lockstep),
 * optionally dumps frames as PPM (<prefix>_<frame>.ppm), and optionally renders
 * the audio offline to a 16-bit mono WAV (single-threaded, so the synth's event
 * ring is driven in lockstep with the game).
 *
 *   usage: x68game_headless [frames] [ppm_prefix] [out.wav]
 */
#include "game.h"
#include "vhw.h"
#include "synth.h"
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

static void write_wav(const char *path, const int16_t *pcm, int nsamp)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    uint32_t data = (uint32_t)nsamp * 2, riff = 36 + data, rate = SYNTH_SR;
    uint32_t byterate = rate * 2, fmtsize = 16;
    uint16_t fmt = 1, ch = 1, align = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtsize, 4, 1, f); fwrite(&fmt, 2, 1, f);
    fwrite(&ch, 2, 1, f);   fwrite(&rate, 4, 1, f); fwrite(&byterate, 4, 1, f);
    fwrite(&align, 2, 1, f);fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);fwrite(&data, 4, 1, f);
    fwrite(pcm, 2, nsamp, f);
    fclose(f);
    printf("wrote %s (%d samples)\n", path, nsamp);
}

int main(int argc, char **argv)
{
    g_total = (argc > 1) ? atoi(argv[1]) : 300;
    const char *prefix = (argc > 2) ? argv[2] : NULL;
    const char *wav    = (argc > 3) ? argv[3] : NULL;

    int16_t *pcm = NULL;
    int nsamp = 0;
    if (wav) pcm = malloc((size_t)g_total * SYNTH_FRAME_SAMP * sizeof(int16_t));

    game_init();
    while (1) {
        uint16_t pad = hal_input();
        game_step(pad);                       /* pushes audio events */
        if (wav) { synth_render(pcm + nsamp, SYNTH_FRAME_SAMP); nsamp += SYNTH_FRAME_SAMP; }
        int cont = hal_vsync();
        if (g_frame == 1 || g_frame == 100 || g_frame == 200 || !cont)
            printf("frame %4d  fbhash=%08x  audiohash=%08x  spr=%d  max/line=%d\n",
                   g_frame, fnv1a(g_fb, sizeof(g_fb)), synth_event_hash(),
                   vhw_last_sprite_count(), vhw_last_max_per_line());
        if (prefix && (g_frame == 90 || g_frame == 180 || g_frame == 270 || !cont))
            write_ppm(prefix, g_frame);
        if (!cont) break;
    }
    if (wav) { write_wav(wav, pcm, nsamp); free(pcm); }
    return 0;
}
