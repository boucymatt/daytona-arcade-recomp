# PSP-1000 frontend

Experimental native PSP port. The i960/TGP game code is statically recompiled
to Allegrex code on the build host; no CPU interpreter or emulator is shipped.
The PSP CPU rasterizer and tile/HUD compositor draw directly into a 480x272
framebuffer, then PSP GU presents RGB565 pixels 1:1 at 480x272. There is no
496x384 intermediate screen or 2x output. Guest coordinates and ROM texture
layouts are retained so gameplay and material mapping stay unchanged. Native
audio has its own device-clocked thread. This is a starting port, not a claim of smooth PSP-1000 gameplay or a finished GPU renderer.

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
The 4:3 option uses a 363x272 content area inside the native 480x272 canvas;
stretch uses its full width. 512-pixel VRAM stride and power-of-two texture
storage are PSP hardware alignment requirements, not a higher display size.
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

The native 480x272 build completed a 600-frame smoke test in PPSSPPSDL 1.20.4
using original-PSP/32 MB mode. Its final hash `a5103ec1c6a9f12d` matches the
corresponding native-resolution host replay. It took 176.566 seconds (about
3.40 game frames/s including startup), compared with 244.214 seconds for the
previous 496x384-rendering build: 27.70% less elapsed time. These are emulator
measurements, not physical PSP performance.

The test confirmed 480x272 rendering, 333/166 MHz CPU/bus clocks and 1,081,344
bytes of GE buffer storage. Final malloc arena use was 16,942,248 bytes;
kernel free memory was 1,306,624 bytes. Audio shut down without errors, but
378 over-budget audio blocks and emulator underruns remained. This build is
still not smooth. Physical PSP-1000 operation, long races and save/menu behavior
need testing; lowering resolution alone does not solve every CPU/storage stall.

Run host regression tests with `ctest --test-dir build --output-on-failure`.
Synthetic tests cover cache wrapping, eviction, short-read failure, sample
reader equivalence, controls and audio queue/lifetime handling. Real-ROM
comparisons use your private imported files:

```sh
bash scripts/test_psp_memory.sh build 6000 build/rom_cache/daytona93 race
SKIP_BUILD=1 bash scripts/test_psp_memory.sh build 1680 build/rom_cache/daytona93 attract 1440 240
PSP_NATIVE_VIDEO=1 SKIP_BUILD=1 bash scripts/test_psp_memory.sh build 600 build/rom_cache/daytona93 attract 0 600
```

The second command checks 240 visible original-resolution attract frames;
the third checks all 600 frames at native 480x272. Synthetic native rendering
tests additionally compare HUD/tile samples against the unchanged reference and
check perspective texture mapping and polygon clipping. Different resolutions
have different framebuffer hashes; these host checks do not replace MAME or physical PSP testing.

Normal file logging is off. Fatal loading, memory, I/O or audio errors appear
in the menu and `psp-fault.log`. If an active game makes no main-thread progress
for five seconds, a small observer writes one `psp-stall.log` record for that
incident. It records the last phase, completed frames/fresh 3D updates, kernel
thread status and atomic audio counters without touching live game objects or
restarting anything. Loading and paused menus are excluded. This distinguishes
a CPU-side stall from a GU/vblank wait if the freeze recurs.

The higher-priority audio worker explicitly yields for a requested 250-1000
microseconds after every successful output block. An empty SRC queue may make
its nominally blocking call return immediately, so relying on that call alone
can starve the game. All samples and game updates are retained; this guard is
not a claim that rendering now reaches full speed. The smoke report includes
`audio_output_call_us`, `audio_fairness_yields` and `audio_fairness_delay_us`.
Test04 completed 600 emulator frames with 419 fresh 3D updates, expected native
image hash and no watchdog errors/incidents, but a subsequent physical PSP test
reported a shutdown after gameplay started, before any log appeared. Do not
treat that build as hardware-stable or keep retrying it.

Test05 fixes an independently verified logging defect: the observer used a
relative native file path without a worker working directory. Paths are now
captured as absolute paths on main before workers start. It also keeps the
audio engine/SRC alive after a failed join until worker completion is certain,
and retains failed-to-delete thread IDs for cleanup. Neither finding establishes
the cause of the hardware shutdown. No stack/VRAM overflow was demonstrated in
the binary audit, and no kernel exception hook or firmware change was added.

For diagnostic testing, put `psp-diagnostics.txt` containing `1` beside EBOOT.
The private test05 package already includes it. This enables
`psp-diagnostic.log`: checked startup checkpoints before graphics, ROM loading,
audio startup and the first game frame, then snapshots approximately once per
second during active gameplay. They include phase/frame counts, thread status,
stack-fill high-water free counts, audio counters, main-published heap samples
with their frame number, and kernel free/largest-block sizes. Stack-fill counts
are estimates, not proof against all stack corruption.

Each native append is closed and the device is synced; existing records are
never truncated. Each log is capped at 2 MiB. Copy logs off the card before
moving/clearing a full log; diagnostic gameplay is refused/stopped on path,
write, sync or observer errors instead of silently continuing without evidence.
Unexpected error text is checkpointed before audio teardown. Syncs add storage
overhead, so this mode is not for performance benchmarking. A true power loss
can still lose the last write or damage the filesystem; earlier checkpoints
are evidence, not a crash dump or a guaranteed recovery mechanism.

Keep `psp-settings.ini`, cabinet save files and the existing `roms/` folder
when updating. To disable proactive logging later, remove the marker file.
The normal fault-only observer remains. The menu reports available heap and
unsupported audio commands. Never ignore a fault by increasing caches blindly.

For an explicit cold-boot smoke test, place `smoke.txt` beside EBOOT with two
integers: frame count (1-6000), presentation skip (0-3), for example `240 3`.
A test-only optional third integer delays the first main frame by that many
milliseconds (capped at 10000); `60 0 6000` deliberately exercises stall recording
while audio continues. This is not used in install packages or normal play.
It autostarts without controller input or saved cabinet state, writes progress
to `smoke-progress.txt`, and writes `smoke-result.txt` when finished. Audio
render-time and over-budget counters help distinguish mixing from board work;
`board_wall_us` includes preemption by audio. `heap_free` means unused space
inside the malloc arena, not total remaining physical RAM. Remove
`smoke.txt` before normal play. Use PPSSPP's original PSP model (`PSPModel=0`)
to test the RAM budget: stock PPSSPPHeadless selects the larger-memory Slim
model internally and is not a PSP-1000 memory test. Emulator speed is not
physical PSP performance proof. Set the emulator window to 480x272 with
`--windowed --xres 480 --yres 272` for a 1x preview; a 960x544 capture is only
a 2x preview and does not mean the PSP target is rendering at Vita resolution.
