/*
 * main_sdl.c - PC/Mac backend. SDL2 is used ONLY as a thin media layer:
 * a single streaming texture for the 256x256 framebuffer, keyboard input, and
 * (later) an audio callback. All rendering is done by the software virtual
 * hardware in src/vhw, so the window is pixel-identical to a headless render.
 *
 * Keys: arrows = move, Z = A, X = B, Enter = Start, Esc/close = quit.
 */
#include "game.h"
#include "vhw.h"
#include "synth.h"
#include <SDL.h>
#include <stdio.h>

#define SCALE 3

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture  *g_tex;
static uint32_t      g_fb[VHW_SCREEN_W * VHW_SCREEN_H];
static int           g_quit;

uint16_t hal_input(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) g_quit = 1;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) g_quit = 1;
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint16_t pad = 0;
    if (k[SDL_SCANCODE_UP])     pad |= PAD_UP;
    if (k[SDL_SCANCODE_DOWN])   pad |= PAD_DOWN;
    if (k[SDL_SCANCODE_LEFT])   pad |= PAD_LEFT;
    if (k[SDL_SCANCODE_RIGHT])  pad |= PAD_RIGHT;
    if (k[SDL_SCANCODE_Z])      pad |= PAD_A;
    if (k[SDL_SCANCODE_X])      pad |= PAD_B;
    if (k[SDL_SCANCODE_RETURN]) pad |= PAD_START;
    return pad;
}

int hal_vsync(void)
{
    vhw_render_rgba(g_fb);
    SDL_UpdateTexture(g_tex, NULL, g_fb, VHW_SCREEN_W * sizeof(uint32_t));
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);          /* vsync-throttled (present-vsync flag) */
    return !g_quit;
}

static void audio_cb(void *user, Uint8 *stream, int len)
{
    (void)user;
    synth_render((int16_t *)stream, len / (int)sizeof(int16_t));
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SYNTH_SR; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 1024;  want.callback = audio_cb;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);
    else fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
    g_win = SDL_CreateWindow("X68000 game (SDL)",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             VHW_SCREEN_W * SCALE, VHW_SCREEN_H * SCALE, 0);
    g_ren = SDL_CreateRenderer(g_win, -1,
                               SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              VHW_SCREEN_W, VHW_SCREEN_H);

    game_init();
    while (1) {
        uint16_t pad = hal_input();
        game_step(pad);
        if (!hal_vsync()) break;
    }

    if (audio) SDL_CloseAudioDevice(audio);
    SDL_DestroyTexture(g_tex);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    return 0;
}
