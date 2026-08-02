# X68000-game-platform

Multiplatform development environment for a new **Sharp X68000** Metroidvania.
Develop and test-play fast on PC/Mac; build for real X68000 hardware from the
same source. See **[DESIGN.md](DESIGN.md)** for the architecture.

The game logic is portable, deterministic C that only calls the HAL
(`src/hal/hal.h`). A software model of the X68000 video hardware (`src/vhw`)
renders it identically on every backend, so the PC window matches real hardware.

## Build & run (PC/Mac)

Requires CMake and a C compiler. For the graphical build, install SDL2:

```sh
brew install sdl2 cmake        # macOS
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/x68game_sdl            # graphical (arrows = move, Esc = quit)
```

### Headless / CI build (no SDL)

Always built. Runs a scripted input sequence, prints framebuffer hashes used
for parity checks, and can dump a frame as a PPM image:

```sh
./build/x68game_headless 180 frame.ppm
ctest --test-dir build         # parity smoke test
```

## X68000 build

Planned via the [elf2x68k](https://github.com/yunkya2/elf2x68k) toolchain
(`-m68000`), run in the [XEiJ](https://stdkmd.net/xeij/) emulator. Not wired up
yet — see the status list in DESIGN.md.

## Layout

| Path | Contents |
|---|---|
| `src/game/` | Shared game logic (portable, 8.8 fixed-point, no float) |
| `src/hal/` | Hardware abstraction layer — the only API the game calls |
| `src/vhw/` | Software X68000 video hardware (renders to a framebuffer) |
| `src/plat_sdl/` | PC/Mac backend (SDL2 as a thin media layer) |
| `src/plat_headless/` | SDL-free backend for CI / parity tests |
| `src/plat_x68/` | X68000 backend (planned) |
