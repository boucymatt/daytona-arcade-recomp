# PSP-1000 frontend

Experimental native PSP port. The i960/TGP game code is statically recompiled
to Allegrex code on the build host; no CPU interpreter or emulator is shipped.
The shared CPU rasterizer draws 496x384, then PSP GU presents an RGB565 image
at 480x272. Native audio has its own device-clocked thread. This is a starting
port, not a claim of smooth PSP-1000 gameplay or a finished GPU renderer.

## Build and install

Use the normal host setup/recompile flow first, with your own verified
`daytona93.zip` or `.7z`. Host tools and generated C++ must exist under `build/`.
Install the public [PSPDEV toolchain](https://github.com/pspdev/pspdev), then:

```sh
python3 scripts/build_psp.py --pspdev /usr/local/pspdev
python3 scripts/prepare_psp.py --rom roms/daytona93.zip
```

Copy `build/psp-install/PSP/GAME/DAYTONA` to `PSP/GAME/DAYTONA` on your Memory
Stick. Keep its `roms/` directory beside `EBOOT.PBP`. A PSP already configured
to run homebrew is required; neither script changes firmware. The preparation
script refuses an existing destination to preserve saves. Use `--output` to
prepare a new folder on subsequent builds, or replace only the old EBOOT after
backing up your install. Do not redistribute this private folder or EBOOT:
both contain game-derived material. Source builds contain no game assets.

For a ROM-free frontend/runtime compile check:

```sh
python3 scripts/build_psp.py --pspdev /usr/local/pspdev --compile-check --build-dir build/psp-check
```

That mode does not link a game and produces no EBOOT. Dependency revisions and
licences are recorded in [THIRD_PARTY.md](THIRD_PARTY.md).

## Controls and options

| Control | PSP input |
| --- | --- |
| Steering | Analog stick or D-pad Left/Right |
| Accelerate / brake | Cross / Square |
| Shift down / up | L / R |
| Insert coin / arcade Start | Select / Start |
| Four views | Circle / Triangle / D-pad Up / Down |
| Pause and options | Start + Select |

Menu controls are Up/Down to select, Left/Right to change, Cross to confirm.
Options include volume, mute, 4:3/stretch, presentation skipping, reset, save
and quit. Settings are written to `psp-settings.ini`; cabinet EEPROM and backup
RAM are saved on pause/reset/quit in `ioboard_eeprom.bin` and `backup_ram.bin`.
Keep those files when updating EBOOT. CPU/bus clocks are fixed
at the supported 333/166 MHz maximum; no firmware overclock is installed.
Display skip reduces GU upload/presentation only: every game frame, geometry
update and CPU raster still runs. It is not a cure for a CPU rendering limit.

## Memory and storage

The target is PSP-1000's 32 MB physical RAM and 24 MiB user partition, which
also contains the executable and thread stacks. The PBP explicitly requests
`MEMSIZE=0`; no expanded-memory firmware setting is needed. ARK-5's
[high-memory implementation](https://github.com/PSP-Arkfive/ARK-5/blob/a9b74547c0d1785cc77981462cf095bd5a76434f/Compat/PSP/src/high_mem.c)
explicitly excludes PSP-1000. It does not turn the Memory Stick into RAM.

Instead, large immutable ROM regions are read through bounded 4 KiB page
caches. Main-board caches total 832 KiB and audio caches total 512 KiB. All
original ROM address masks and read semantics remain intact; mutable board
RAM is not swapped. The low-memory build also uses a sparse address table and
does not allocate unused external-GPU composition layers. Desktop and Vita
retain their existing memory layout by default.

This trades memory for storage traffic. Slow cards, random reads, CPU
rasterization and mixing can cause stalls or audio underruns. Four-way caches
reduced main-board reads in a 6,000-frame host race from 3.623 GB to 1.461 GB,
not to zero. Those numbers include startup, omit audio ROM traffic, and are
not hardware throughput measurements.

## Validation and diagnostics

Current baseline: PPSSPPSDL 1.20.4 in original-PSP/32 MB mode completed a
600-frame smoke test, rendered textured 3D, and shut audio down cleanly. Its
final framebuffer hash matches desktop (`36945e52a376dc48`). However, it took
244 seconds (about 2.46 frames/s including startup), and audio underruns were
observed. This build is not ready to promise smooth gameplay. Physical
PSP-1000 operation, long races and save/menu behavior still need testing.

Run host regression tests with `ctest --test-dir build --output-on-failure`.
Synthetic tests cover cache wrapping, eviction, short-read failure, sample
reader equivalence, controls and audio queue/lifetime handling. Real-ROM
comparisons use your private imported files:

```sh
bash scripts/test_psp_memory.sh build 6000 build/rom_cache/daytona93 race
SKIP_BUILD=1 bash scripts/test_psp_memory.sh build 1680 build/rom_cache/daytona93 attract 1440 240
```

The second command compares every CPU-rendered pixel in 240 visible attract
frames; these host checks do not replace MAME or physical PSP testing.

Normal file logging is off. Fatal loading, memory, I/O or audio errors appear
in the menu and `psp-fault.log`. The menu reports available heap and unsupported
audio commands. Never ignore a fault by increasing cache sizes blindly.

For an explicit cold-boot smoke test, place `smoke.txt` beside EBOOT with two
integers: frame count (1-6000), presentation skip (0-3), for example `240 3`.
It autostarts without controller input or saved cabinet state, writes progress
to `smoke-progress.txt`, and writes `smoke-result.txt` when finished. Audio
render-time and over-budget counters help distinguish mixing from board work;
`board_wall_us` includes preemption by audio. `heap_free` means unused space
inside the malloc arena, not total remaining physical RAM. Remove
`smoke.txt` before normal play. Use PPSSPP's original PSP model (`PSPModel=0`)
to test the RAM budget: stock PPSSPPHeadless selects the larger-memory Slim
model internally and is not a PSP-1000 memory test. Emulator speed is not
physical PSP performance proof.
