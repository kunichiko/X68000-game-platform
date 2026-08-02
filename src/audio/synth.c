/*
 * synth.c - PC software synthesiser. See synth.h. Backend/PC code: float math
 * is fine here (this never runs on the X68000, where the real chips play).
 */
#include "hal.h"
#include "synth.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SR SYNTH_SR

/* --- lock-free SPSC event ring (producer: game thread, consumer: audio) --- */
typedef struct { int kind, a, b, c; } event_t;   /* kind: 0 key,1 off,2 adpcm */
#define ERING 256
static volatile event_t   s_ering[ERING];
static volatile unsigned  s_ehead, s_etail;

/* --- deterministic event digest (producer side) --- */
static uint32_t s_evhash = 2166136261u;
static void hash_ev(int kind, int a, int b, int c)
{
    int v[4] = { kind, a, b, c };
    const uint8_t *p = (const uint8_t *)v;
    for (unsigned i = 0; i < sizeof(v); i++) { s_evhash ^= p[i]; s_evhash *= 16777619u; }
}

static void push(int kind, int a, int b, int c)
{
    hash_ev(kind, a, b, c);
    unsigned n = (s_ehead + 1) % ERING;
    if (n != s_etail) {
        s_ering[s_ehead].kind = kind; s_ering[s_ehead].a = a;
        s_ering[s_ehead].b = b;       s_ering[s_ehead].c = c;
        s_ehead = n;
    }
}

/* --- HAL audio implementation --- */
void hal_ym_key(int ch, int note, int inst) { push(0, ch, note, inst); }
void hal_ym_off(int ch)                      { push(1, ch, 0, 0); }
void hal_adpcm(int id)                        { push(2, id, 0, 0); }

uint32_t synth_event_hash(void) { return s_evhash; }

/* --- voices --- */
typedef struct {
    int active; double cph, mph, freq, ratio, index, env, edec, ienv, idec;
} fmv_t;
static fmv_t s_fm[8];

typedef struct { int active; int type; long pos, len; uint32_t rng; } sfxv_t;
static sfxv_t s_sfx[4];

static double note_hz(int n) { return 440.0 * pow(2.0, (n - 69) / 12.0); }

static void note_on(int ch, int note, int inst)
{
    ch &= 7;
    fmv_t *v = &s_fm[ch];
    double amp_ms = 180, idx_ms = 90;
    v->ratio = 2.0; v->index = 3.0;
    if (inst == INST_JUMP) { v->ratio = 3.0; v->index = 4.0; amp_ms = 160; idx_ms = 70; }
    v->active = 1; v->cph = v->mph = 0; v->freq = note_hz(note);
    v->env = 1.0; v->ienv = 1.0;
    v->edec = pow(0.0008, 1.0 / (amp_ms * 0.001 * SR));
    v->idec = pow(0.02,   1.0 / (idx_ms * 0.001 * SR));
}

static void note_off(int ch) { s_fm[ch & 7].edec = pow(0.0008, 1.0 / (0.05 * SR)); }

static void sfx_on(int id)
{
    long len = (id == SFX_LAND) ? (long)(0.14 * SR) : (long)(0.045 * SR);
    sfxv_t *v = &s_sfx[0];
    for (int i = 0; i < 4; i++) if (!s_sfx[i].active) { v = &s_sfx[i]; break; }
    v->active = 1; v->type = id; v->pos = 0; v->len = len;
    v->rng = 0x1234567u + (uint32_t)id * 2654435761u;
}

static void drain_events(void)
{
    while (s_etail != s_ehead) {
        event_t e = { s_ering[s_etail].kind, s_ering[s_etail].a,
                      s_ering[s_etail].b, s_ering[s_etail].c };
        s_etail = (s_etail + 1) % ERING;
        if      (e.kind == 0) note_on(e.a, e.b, e.c);
        else if (e.kind == 1) note_off(e.a);
        else if (e.kind == 2) sfx_on(e.a);
    }
}

void synth_render(int16_t *out, int nframes)
{
    drain_events();
    for (int i = 0; i < nframes; i++) {
        double s = 0;
        for (int c = 0; c < 8; c++) {
            fmv_t *v = &s_fm[c];
            if (!v->active) continue;
            double m = sin(2 * M_PI * v->mph) * v->index * v->ienv;
            s += sin(2 * M_PI * v->cph + m) * v->env * 0.25;
            v->cph += v->freq / SR;            if (v->cph > 1) v->cph -= 1;
            v->mph += v->freq * v->ratio / SR; if (v->mph > 1) v->mph -= 1;
            v->env *= v->edec; v->ienv *= v->idec;
            if (v->env < 0.0009) v->active = 0;
        }
        for (int c = 0; c < 4; c++) {
            sfxv_t *v = &s_sfx[c];
            if (!v->active) continue;
            v->rng = v->rng * 1664525u + 1013904223u;
            double nz = ((int)((v->rng >> 9) & 0x7fff)) / 16384.0 - 1.0;
            double e = 1.0 - (double)v->pos / (double)v->len; if (e < 0) e = 0;
            if (v->type == SFX_LAND) {
                double th = sin(2 * M_PI * 80.0 * v->pos / SR);
                s += (nz * 0.4 + th * 0.6) * e * e * 0.5;
            } else {
                s += nz * e * e * 0.3;
            }
            if (++v->pos >= v->len) v->active = 0;
        }
        if (s >  1) s =  1;
        if (s < -1) s = -1;
        out[i] = (int16_t)(s * 30000.0);
    }
}
