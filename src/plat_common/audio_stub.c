/*
 * audio_stub.c - no-op audio HAL, linked into backends that do not yet drive
 * sound. Replaced by real YM2151 / ADPCM implementations next milestone
 * (ymfm on PC, IOCS OPMSET / ADPCM on X68000).
 */
#include "hal.h"

void hal_ym_key(int ch, int note, int inst) { (void)ch; (void)note; (void)inst; }
void hal_adpcm(int id)                       { (void)id; }
