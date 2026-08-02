/*
 * synth.h - PC/Mac software synthesiser backend.
 *
 * Implements the audio HAL (hal_ym_key / hal_ym_off / hal_adpcm) and renders
 * PCM. This is a *placeholder timbre* (a small 2-operator FM voice plus
 * procedural noise SFX), NOT a faithful YM2151 model: faithful FM comes from
 * the real chip on the X68000 build (and ymfm later on PC). What IS already
 * faithful is the *event stream* — which notes/SFX fire on which frame — which
 * is produced deterministically by the shared game logic. synth_event_hash()
 * exposes a running hash of that stream so the audio path can be parity-checked
 * just like the framebuffer.
 *
 * Threading: HAL calls (game thread) only push to a lock-free event ring;
 * synth_render() (audio thread) drains it. No shared voice state is touched
 * across threads.
 */
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#define SYNTH_SR         44100
#define SYNTH_FRAME_SAMP (SYNTH_SR / 60)   /* samples per 60Hz game tick */

void     synth_render(int16_t *out, int nframes);  /* mono S16 */
uint32_t synth_event_hash(void);                   /* deterministic event digest */

#endif /* SYNTH_H */
