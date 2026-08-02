# X68000 Metroidvania — Multiplatform Development Environment

A new Metroidvania for the Sharp X68000, developed and test-played fast on
PC/Mac, then built for real hardware.

## Core approach: "X68000 is the spec"

The PC/Mac build is **not a general game engine** — it is a software emulator of
the X68000's video/audio programming model. The game logic never touches SDL,
floats, or modern GPU features; it only calls the HAL, whose primitives *are*
X68000 hardware primitives. The PC backend reproduces the real hardware budgets
(128 sprites, 32/scanline, 16×16-color palette) so it is impossible to build
something that won't fit on real hardware.

Rejected alternatives:

- **Develop on an emulator only** — faithful, but slow build/debug iteration.
- **Full portable engine, port later** — you inevitably use capabilities the
  X68000 lacks, and the port becomes a rewrite.

## Layers

```
src/game/       Shared game logic. Portable C, 8.8 fixed-point, no float,
                deterministic. Compiled identically for PC/Mac and X68000.
src/hal/        The only API the game may call (hal.h). Each call = one
                X68000 hardware primitive.
src/vhw/        Software model of the X68000 video hardware. Implements the
                video HAL and rasterises to an RGBA framebuffer. Single source
                of truth for "what the X68000 shows".
src/plat_sdl/       PC/Mac backend: SDL2 as a thin media layer (framebuffer
                    texture + input + audio). Rendering is all vhw.
src/plat_headless/  SDL-free driver for CI / parity checks. Scripted input,
                    framebuffer hashing, PPM dump.
src/audio/          PC software synth: implements the audio HAL, renders PCM.
src/plat_x68/       (next) X68000 backend via IOCS/DOS on the real chips.
tools/          Aseprite/Tiled → native binary asset converters (planned).
tests/          Input-replay → state-hash parity tests (planned).
```

## Confirmed decisions

| Topic | Decision |
|---|---|
| Target | Base X68000 (68000 @ 10MHz, 2MB) as the floor; staged to real hardware. |
| Language | C99, 8.8 fixed-point in shared logic, **no float** (bit-identical sim). |
| PC/Mac | clang + SDL2 (thin layer) + ymfm for YM2151 (audio: next milestone). |
| X68000 | elf2x68k toolchain, `-m68000`, IOCS/DOS for the real chips. |
| Emulator | XEiJ (manual visual/audio check on the Mac). |
| CI verify | run68 (headless) — replay recorded input, compare state hash to SDL. |
| Assets | Authored in Aseprite/Tiled → one native binary format read by *both* sides. |

## Why 8.8 fixed-point

The base 68000 has no FPU, and MULU/MULS are only 16×16→32. A 16.16 multiply
becomes a slow 32×32 library call. 8.8 keeps common multiplies inside a single
16×16 MULU. Using the *same* fixed-point on PC and X68000 makes the simulation
deterministic, which enables record-input / replay-and-compare parity testing.

## Verification loop

- **run68 (headless, CI):** feed a recorded input sequence, dump a state/frame
  hash, compare against the SDL build. Guards determinism automatically.
- **XEiJ (manual, desktop):** confirm real sprite/scroll/palette/YM2151 behaviour
  the headless path can't show. Needs a JRE and user-supplied IPLROM/CGROM.

## Audio

Same "thin media layer, testable logic" split as video. The synth
(`src/audio/synth.c`) is PC-only backend code (float allowed) that implements
the audio HAL and renders PCM; SDL just feeds an audio device from it. HAL calls
push to a lock-free ring (game thread → audio thread) and accumulate a
deterministic `synth_event_hash()`, so *which* notes/SFX fire on *which* frame is
parity-checked exactly like the framebuffer. The synth's *timbre* is a
placeholder (2-op FM + procedural noise SFX); faithful sound is the real YM2151
on X68000, with ymfm as the drop-in PC upgrade. The headless driver can render
the audio offline to a WAV (single-threaded, lockstep with the game).

## Status

- [x] HAL defined; software virtual hardware (BG tilemap + 128 sprites + palette).
- [x] Platformer vertical slice: gravity, jump, tile collision, camera scroll.
- [x] Headless backend builds & renders; framebuffer + audio-event parity hashes.
- [x] SDL backend (video + audio), builds on a machine with SDL2.
- [x] Audio: placeholder FM/SFX synth on PC; deterministic event stream.
- [ ] Audio fidelity: ymfm YM2151 on PC, IOCS OPM/ADPCM on X68000.
- [ ] X68000 backend (elf2x68k) + XEiJ run.
- [ ] Asset pipeline (Aseprite/Tiled → native binary).
- [ ] Gameplay: enemies, hazards, map transitions, save.

## Building

See [README.md](README.md).
