# Handoff

## PSP per-stage and ROM I/O profiling (2026-10-01)

The new physical test05 psp-diagnostic.log has 44 heartbeats and ends with
shutdown_before_audio_close and shutdown_audio_closed at frame188, not an
abrupt missing-log cutoff. It proves audio teardown completed for that run,
not why the application exited or what happened afterward. No caught exception,
audio failure, five-second stall or recorded memory exhaustion appeared.
Pre-3D progress was about5.14fps; first3D progress was only0.61-0.65fps.
Last active heap free1,029,464B, kernel free1,159,168B; main/audio/observer
stack-fill free counts221,728/130,028/12,740B. Main's lower watermark than
the smoke test already appeared during NV loading (normal play loads saves).
Audio starts5-11voices with3D and rises from~0.5ms to4-11ms/block,
peaking37.772ms with34late blocks. Neither cache growth nor overclocking was
justified by these measurements.

Both main and audio sometimes waited on UID34402069. A numeric wait UID alone
does not identify storage or a driver. Installed SDK read/seek wrappers do not
hold the descriptor-allocation lock around I/O; FILE handles have their own
locks, and unbuffered fread has a bulk-read path. Do not re-propose4096 single-
byte reads or a confirmed global FILE lock as the cause from that log.

Test06 implements the requested measurements, not an optimization:
- Existing GameLoop/Video clocks enabled for PSP diagnostic mode. Completed
  frame snapshots separate core/i960+synchronousTGP remainder, geometry, video,
  raster, tile-cache, tile-draw, composition and presentation wall microseconds.
- Main-board ROM seek/read time and successfully read page counts per frame.
- Separate program/main_data/copro_data/polygons/textures/pcm1/pcm2 counters.
  A default-null PagedRom observer wraps only cache-miss seek/read calls,
  reports success/failure, and never changes cache contents/address semantics.
  Hits do not call clocks. Desktop/Vita do not enable this hook.
- Owner-thread callbacks publish32-bit atomics into fixed PSP storage.
  Watchdog never reads live caches, FILEs, engine or game profiles. Completed
  frame snapshots use a bounded sequence check (never spin on preempted main).
  Per-region live phase/counters are best-effort independent samples.
- Wall times include preemption/wait/instrumentation. ROM I/O is already
  included in its caller's stage; do not sum it again. Per-ROM microsecond
  totals wrap modulo2^32; successful pages are4096B. Counters accumulate across
  resets; repeated profile_frame values are the same sample.
- Shutdown checkpoints record system callback/menu/smoke exit path, without
  claiming that an exit callback identifies a physical poweroff's cause.
- All profiling is opt-in via existing psp-diagnostics.txt=1. Marker-off normal
  game behavior remains unchanged. Diagnostic logging still caps files2MiB.

Validation:
- Full desktop build and PSP cross-build pass. CTest:31passed,2optional
  Lua tests skipped; the final paged-ROM regression was rebuilt and rerun.
- I/O ordering, no callbacks on hits/disabled observers, truncated-read failure
  preservation, successful/failed timing events and32-bit clock wrap covered.
- Bounded coherent snapshots stress-tested with100,000 concurrent publications.
  Profile tests pass optimized, ASan/UBSan and TSan; paged-ROM tests pass
  optimized and ASan/UBSan.
- Original-PSP/32MB emulator:600frames,419fresh3D,182.701965s,
  hash a5103ec1c6a9f12d unchanged;16,740audio blocks, no audio/log errors
  or watchdog incidents,190journal records before shutdown. Normal test
  completion records exit_reason=3. Heap used/free16,942,360/706,792B,
  kernel free after audio pause1,290,240B. Observer stack-fill free10,636B
  with the larger bounded4096B diagnostic formatter.
- Example emulator frame370: board357364us, video283716us, raster173757us,
  tile-cache70969us, geometry59984us, main-ROM seek767us/read13748us,
  126pages. These are NOT physical PSP bottleneck measurements.
- Emulator evidence: build/psp-emulator/psp-diagnostic-test06-600.log and
  smoke-{result,progress}-test06-600.txt. No game-derived artifacts are tracked.

Private package: build/psp-test06/PSP/GAME/DAYTONA with checked ROMs and marker,
no smoke.txt or user saves. Update archive build/psp-test06-update.zip contains
EBOOT.PBP, marker and instructions only. Keep existing ROMs/settings/saves.
EBOOT4,540,882B SHA256
351e7001ac5ec522c06551f8398af68a8c3f43bdef3587c49b70b3111d20bb89.

Next: use a physical test06 log with active3D to separate CPU raster/tile work
from main/audio ROM I/O pressure before making a targeted performance change.
Smooth physical PSP gameplay and the previously reported poweroff remain
unresolved. Resolution480x272, memory budget, clocks and audio engine unchanged.


## PSP shutdown evidence capture (2026-10-01)

Physical feedback supersedes test04's emulator success: gameplay began, then
the PSP powered off before a log appeared. Stop using test04. The physical
shutdown cause remains unconfirmed; test05 is a diagnostic build, not a
hardware stability/performance claim. Work stays on psp-native-frontend.
Desktop/Vita rendering and native audio are unchanged.

Concrete logging defect: the old observer called native
sceIoOpen("psp-stall.log") with a relative path. PSPSDK newlib _open expands its
global cwd through __path_absolute, but direct native worker I/O does not.
PPSSPP v1.20.4 MetaFileSystem.cpp tracks kernel cwd per thread and returns
0x8002032c when a new worker has none. The healthy test04 smoke never called
the error-only writer, so its watchdog_error=0 did not validate the path.
Additionally, a five-second stall detector cannot capture an earlier poweroff.

Test05 captures checked main getcwd before workers start and builds absolute
native diagnostic/stall paths. The fixed-storage helper handles short writes,
zero progress, negative native errors, seek/close/sync failures and a 2 MiB
per-file cap. Writes append, close and sync the device; prior records are never
truncated. Atomic error/record counters are safe for concurrent observation.
This improves capture, not physical power-loss atomicity or filesystem safety.

The opt-in psp-diagnostics.txt marker containing 1 enables startup checkpoints
before callbacks, graphics, ROM load, board/audio setup and the first game
frame, then approximately one-second active-game snapshots. Main is the sole
writer while the observer is stopped, and joins it before load/reset/fault/
shutdown checkpoints. Caught error text is saved before audio teardown. The
observer uses published progress/audio/heap atomics and kernel thread/status/
stack-fill queries, never live game/renderer/heap access or GU calls.
Heap samples include their main-frame index. Main publishes the audio worker
ID; observers do not read Audio's non-atomic lifecycle fields. Diagnostic
path/write/sync/observer failures refuse or stop gameplay, retaining the logs.
Normal releases omit the marker and retain only the fault-only observer.
Sync overhead makes diagnostic runs unsuitable as performance benchmarks.

A separate concrete latent lifetime defect was hardened: Audio::pause ignored
kernel join/delete results. A failed join now keeps SRC/engine state until a
worker-finished atomic, published after the last object access. A failed
deletion retains the ID and rejects reopening until cleanup succeeds. No
forced termination or premature object destruction is used; a truly stuck
worker can still wait indefinitely. This failure path was reproduced in host
tests, not in the reported device poweroff.

Rejected/unproven hypotheses: actual test04 MIPS frames were 21,224 B main
(256 KiB stack), 16,400 B loading NV helper, 4,176 B Video draw, at most 440 B
raster, at most 48 B generated chunks/TGP, 96 B audio entry plus at most 168 B
audio helpers (128 KiB stack), and 1,824 B observer record (16 KiB stack).
Generated guest calls are state updates/tail dispatch, not growing native
recursion. Linked _sbrk is bounded in a separately allocated kernel heap block;
it does not grow into stacks. Linked newlib has real lwmutex malloc/FILE locks
and pthread glue initialization, not dummy locks. Distinct PCM files/caches are
worker-owned. The global reent pointer alone is not evidence of corruption.
The two static 2048 B SRC buffers are 64 B aligned and written back before
submission. GU commands/vertices use SDK uncached aliases; the 1,081,344 B
VRAM allocation and small draw list are within bounds. None of these static
checks proves absence of runtime corruption. No clock increase, RAM patch,
kernel-mode exception handler or firmware change was used.

Validation:
- Diagnostic-log tests: absolute worker paths, malformed/long cwd, short/zero/
  negative writes, failure preservation, append cap, seek/close/sync errors,
  and concurrent counter reads. Optimized, ASan/UBSan and TSan pass.
- Audio tests: join failure while a worker remains inside render, pause and
  close retention, wait failure, failed deletion/retry; optimized, ASan/UBSan
  and TSan pass. Normal scheduling/audio test cases remain green.
- CTest: 30 passed, 2 optional Lua tests skipped. PSP cross-build passes.
- Deliberate first-frame 6000 ms main delay (smoke.txt: 60 0 6000) in
  PPSSPPSDL original-PSP mode saved startup/periodic records and exactly one
  stall while audio continued: stalled_us=5052226, frames=0, audio_blocks=499.
  The run recovered and completed 60 frames with no audio/log errors.
  Native writer/sync succeeded, not merely compiled. This does not simulate
  physical card power-loss durability. Archived *test05-injected* logs in
  build/psp-emulator retain the evidence.
- Marker-off forced-stall retest: 60 frames completed, exactly one native
  stall record, no psp-diagnostic.log, diagnostic_enabled=0, errors=0. This
  checks the normal fault-only path separately from proactive mode.
- Final test05 original-PSP/32 MB emulator run: 600 frames / 419 fresh3D in
  182.617794 s, expected native hash a5103ec1c6a9f12d, 16,711 audio blocks,
  zero audio/log errors and no stall incidents. 190 journal records before
  shutdown plus two shutdown checkpoints. Final heap used/free:
  16,942,248/767,320 B; kernel free after audio pause: 1,290,240 B.
  Active stack-fill free estimates: main 234,624/262,144 B, audio
  130,028/131,072 B, observer 12,740/16,384 B. Native 480x272 capture inspected
  at build/psp-emulator/test05-3d.png. 349 over-budget audio blocks and roughly
  3.29 game frames/s remain; neither full speed nor hardware safety is proved.

Private diagnostic install: build/psp-test05/PSP/GAME/DAYTONA, verified ROM
import, diagnostic marker included, no smoke.txt or existing user saves.
build/psp-test05-update.zip contains only EBOOT.PBP, the marker and diagnostic
instructions; copying it must retain the existing ROMs/settings/cabinet saves.
EBOOT: 4,537,650 bytes, SHA256
32cb8f31766f6bb570f869d7bdbef1f64000e05e8f419cfa9e6c2a9a865f4247.

Next: inspect physical test05 startup/last heartbeat and any stall/fault record
before changing renderer, mixer, memory limits or clocks. If a physical unit
powers off again, do not repeatedly retry the build. Full hardware stability
and smooth PSP-1000 gameplay remain unresolved.


## PSP render-freeze scheduling guard (2026-10-01)

A new report says 3D freezes while audio continues. No new device log was
provided. The reported test03 build put audio at priority 0x12 above main at
0x20, with no explicit wait in its successful output loop. The SRC call can return
without blocking when its queue is empty: see PPSSPP v1.20.4
[__AudioEnqueue](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/HLE/__sceAudio.cpp#L176-L206).
If mixing takes longer than the 10.667 ms block, relying on that call alone
can leave audio continuously runnable and starve main. This is a concrete
scheduling weakness consistent with the symptom, not proof of the reported
hardware root cause.

The PSP worker now requests a real timed wait after every successful output:
max(250, 1000 - output_call_elapsed) microseconds for short calls, 250 otherwise.
The first proposed guard only delayed calls shorter than 1 ms; review rejected
that as insufficient because elapsed API time is not proof of scheduler wait.
A 100-us floor was also considered; PPSSPP clamps sub-200-us thread waits to
210 us, so the explicit floor is 250 us. Actual wake time may be later.
Output-call duration, yield count and requested delay are published atomically.
Delay failure becomes a visible audio fault instead of a new tight loop. No
samples, commands, geometry updates or game frames are discarded.

A small PSP-only observer checks main progress every 250 ms, reports once after
five seconds without progress in an active game, and re-arms after recovery.
It records phase, completed frame/fresh-raster counts, kernel thread status and
audio counters to psp-stall.log through native file I/O. It does not inspect
live game/heap objects, touch GU, terminate a thread or alter game state.
Loading and paused menus are excluded; ordinary per-frame file logging stays
off. Observer setup/cleanup is checked and its storage lives through join.

Fresh read-only native 480x272 host checks rendered all 6,000 attract and all 6,000
race frames, comparing every dense/paged framebuffer exactly. Attract digest
cb3350b14d1395db, final a60acc6de958dac5, 153,628,673 i960/143,436,167 TGP
instructions. Race digest 14c33947133a8f6c, final de73b6f16dd18f81,
196,665,345 i960/223,429,779 TGP instructions. Neither froze. The 600-frame
hash remains a5103ec1c6a9f12d. This does not prove physical PSP stability.

The ROM cache audit found no dangling transient page pointers; accesses return
values before subsequent eviction. Audio/main cache ownership is independent.
Raster output loops are bounded at 480x272, and finite edge walks terminate at
the maximum-y vertex. Polygon capacity reaches 4096 before frame 600 and later
maxima remain below it, so that later growth hypothesis was not supported.
An uncapped direct-data sentinel loop on corrupted geometry input was noted,
but no corrupted input or hang was reproduced; no speculative raster/geometry
changes were made.

GU audit at the pinned public PSPSDK found commands and sceGuGetMemory vertices
already use the uncached list alias. Texture uploads use uncached VRAM and
TexFlush. No missing flush was established; GU presents one fully composed CPU
image, so it cannot selectively stall 3D while its HUD continues animating.
The watchdog detects main progress stalls, not an unchanged picture when all
main phases still advance.

Validation: 29 CTests passed and two optional Lua tests skipped.
Audio tests pass optimized, ASan/UBSan and ThreadSanitizer, including a 12-ms
mixer, immediate/partial/normal/slow-nonblocking SRC, timer rollover, delay
errors, sample order and buffer lifetime. Progress tests cover five-second
threshold, one report per incident, recovery, inactive modes and timer wrap.
A fresh unmodified test03 emulator baseline completed 1,800 frames without
freezing, with status=ok in 576.979 seconds, hash e4ce88e5c42f597a. A fresh
native host replay of all 1,800 frames matches that hash, digest
9c9fb47dad176da6, 56,372,225 i960/50,380,430 TGP instructions. Therefore the
reported device freeze was not reproduced; do not call it proven fixed.

Test04 builds to a 4,530,490-byte EBOOT, SHA256
f0313757ce008ac0d23c7fa7cc2d70cd7ac631e210fd4663bb83682429ce5a5b.
Private install folder: build/psp-test04/PSP/GAME/DAYTONA, verified ROM import,
no smoke files. Existing installs can replace only EBOOT.PBP and retain ROMs,
settings and saves. Native 480x272 output, normal 32 MB PSP memory and
333/166 MHz clocks remain unchanged; desktop/Vita runtime sources were not
modified.

Final test04 PPSSPPSDL original-PSP smoke completed 600 frames with every frame
presented, status=ok, hash a5103ec1c6a9f12d and 419 fresh 3D updates. It took
182.408 seconds versus 182.104 seconds at frame600 in the corresponding
unmodified every-frame-presentation baseline: no performance improvement is
claimed. It submitted 16,714 audio blocks with 16,713 explicit yields (final
shutdown block needs no yield), no audio errors/invalid/unsupported commands,
and available watchdog with zero errors/incidents. The emulator still reports
underruns and 335 late blocks (peak 82.401 ms); this is not smooth playback.
Heap used/free: 16,942,248/770,648 bytes; kernel free: 1,290,240 bytes, exactly
16 KiB below the old smoke due to the bounded observer stack. No psp-stall.log
was produced during healthy progress. Result/progress archives are in
build/psp-emulator/smoke-{result,progress}-freeze-test04-600.txt; the inspected
480x272 3D capture is build/psp-emulator/freeze-test04-3d.png.

Next: physical PSP retest of test04, then inspect fault/stall records if the
freeze persists. Keep the distinction between an actual stalled main thread
and unchanged 3D while the game/HUD still advance. No hardware fix or smooth
playback claim is justified by these emulator results alone.

## PSP native 480x272 rendering (2026-10-01)

The PSP-only M2_PSP_NATIVE_VIDEO build now rasterizes geometry and composes
HUD/tile layers directly into a 480x272 canvas. GU converts to RGB565 and
presents 1:1 with nearest sampling; it no longer uploads/scales a 496x384
screen. The original screenshot's 960x544 size was a 2x PPSSPP window, not a
Vita render target. Previews now use an actual 480x272 emulator window too.

The guest coordinate space remains 496x384. Final geometry coordinates and
inclusive viewport bounds map to native output pixel centers, preserving
perspective UV/depth/luma and original checker phase. HUD/tile pixels use
precomputed inverse-center lookups, retaining all original masks, line-scroll,
wrap and priority rules. 4:3 has 363x272 content at x=58; stretch uses 480x272.
GameLoop framebuffer dimensions also advertise the selected native output.
Both modes allocate only native-size output/composition targets. Guest texture
sheets/tilemaps remain their hardware formats, not resized ROM data.

PSP-1000 constraints remain explicit: MEMSIZE=0, 32 MB physical/24 MiB user RAM,
333 MHz CPU/166 MHz bus supported maximum, 1,081,344 bytes GE memory use below
its 2 MiB budget. The 512-pixel stride/power-of-two upload texture is PSP alignment,
not Vita resolution. Mutable RAM, original game timing and command sequences
are not reduced or skipped. Normal logging stays off; smoke reports include
render dimensions, actual clock readings and buffer usage.

Validation before hardware:

- 28 host CTests passed; two optional Lua tests skipped.
- Native raster optimized and ASan/UBSan tests cover 480x272 bounds, clipped
  triangles/quads, both aspects, checker phase and perspective texture values.
- 33,423,360 synthetic native HUD/tile pixels exactly match the untouched
  reference sampled at native pixel centers (128 randomized scenes, both
  aspects, all scroll/window/category modes); ASan/UBSan passed.
- 1,044,480 pixels from real-game 2D/HUD captures at frames 600, 900, 1440, 1560
  match the independently compiled reference in both aspects. Native full
  captures were visually inspected for roads, cars, HUD and logos.
- Native 1680-frame dense/paged replay with 240 visible renders preserves the
  same 52,998,145 i960/46,157,202 TGP instructions as the 496x384 default.
  Native final hash bba1eaa8d6788067, reference c24dbbad1b5f898f: differing
  resolution means those image hashes SHOULD differ; guest state does not.
- Native 600-frame full-render replay: hash a5103ec1c6a9f12d, 600 framebuffer
  comparisons, 419 fresh raster updates, 596 nonblack frames. This is the
  corresponding native-resolution PSP smoke oracle, not the older 36945e52...
- Desktop/Vita optimized/reference comparisons pass all 160 randomized screen
  scenes and 640 raster scenes. Their resolution and default paths are unchanged.
  Fixed the comparison script's namespace clone to include paged_rom.h; it
  previously failed on PagedRom declarations before any pixel comparison.

The resolution change saves 487,168 bytes in Video screen/composition storage
and 657,920 bytes in Raster color/fill storage, less small coordinate tables.
Final isolated PPSSPPSDL original-PSP smoke completed 600 frames in 176.566
seconds (3.40 game frames/s including startup), versus 244.214 seconds before:
27.70% less elapsed time, not a real-hardware performance measurement.
The native hash a5103ec1c6a9f12d exactly matches the corresponding host replay.
Reported output is 480x272, clocks 333/166 MHz, VRAM 1,081,344 bytes; final
malloc arena used 16,942,248 bytes, free 770,392, kernel free 1,306,624.
Audio shuts down without errors, invalid or unsupported commands, but 378
blocks exceed 10.667 ms (peak 97.039 ms) and emulator underruns remain.
This is NOT smooth playback or physical PSP-1000 validation.

The first native-size implementation took 194.316 seconds overall but made
the early 2D-heavy boot slower: frame 180 was 29.396 seconds, versus 23.916
seconds in the original build. Per-pixel window-mask work was avoidable.
Grouping copies by the original 128-pixel mask word and separating opaque,
transparent, all-masked and mixed cases preserves exact samples and removes
repeated mask arithmetic. Final frame 180 is 20.997 seconds. All six controlled
host mask-benchmark hashes stayed identical, and the complete native replay
hash and instruction/cache counts also stayed unchanged. Do not assume a
resolution reduction automatically makes every phase faster.

The final PSP EBOOT is 4,525,562 bytes, SHA256
cbf30caf0f4725258421c9c78be39b98ff11315816bc90e101e1e6a6f4763961.
PRX loaded size is 0x3c79b0. The ready private install folder is
build/psp-test03/PSP/GAME/DAYTONA, with verified ROMs and no smoke files.
The final result is archived at
build/psp-emulator/smoke-result-native-480-test03.txt.
The actual 480x272 3D capture is
build/psp-emulator/psp-native-render-480x272.png (before the pixel-identical
mask optimization). The post-test screenshot is only the emulator menu,
not gameplay evidence.

The shared sources also rebuild/package successfully for Vita ARM with
logging OFF and without M2_PSP_NATIVE_VIDEO. The existing GPU25 archive remains
untouched. Final build/vita-gpu10/daytona_vita.vpk passes archive validation,
SHA256 88dbc61cba203c70e13296e0f4730f3082a26dbb89d3fad866220fdc17c59b07.
No new physical Vita test was performed. The bash-runner contract test skips
non-POSIX/no-bash hosts; four tests pass on this host.

Next: profile the remaining PSP tile-cache rebuilds, CPU rasterization and
ROM I/O, then validate a full race, audio, menus, saves and shutdown on an
actual PSP-1000. The original baseline and limitations remain recorded below.

## Experimental PSP-1000 frontend (2026-10-01)

The completed Vita/native-audio work was fast-forwarded to main and pushed to
GitHub; main/origin main is `8159b0135f3652ee10fd3876f6c21e1db7351118`.
PSP development is isolated on `psp-native-frontend`, not merged back into main.

The new `platform/psp` frontend builds with public PSPDEV/PSPSDK to a private
EBOOT.PBP. It statically recompiles the existing game/TGP code, uses native
PSPSDK GU presentation/input, and runs the shared native 48 kHz audio engine
on a dedicated thread. No SDL, interpreter, firmware patch or game-derived
source/data is committed. CPU/bus clocks are 333/166 MHz. Controls, pause menu,
volume/mute, aspect, presentation skip and cabinet/settings persistence are
implemented. Native audio remains experimental; there is no reference-backend
fallback on PSP.

PSP-1000 has 32 MB physical memory and a 24 MiB user partition shared by code,
heap and stacks. ARK-5's high-memory source explicitly excludes PSP-1000;
it does not provide SD-card swap. The port instead pages immutable ROMs through
4 KiB read-only caches: 832 KiB main-board and 512 KiB audio payload, separate
ownership/files per thread. Mutable board RAM remains resident. M2_LOW_MEMORY
uses sparse page tables and avoids unused external-GPU layer allocations.
Default desktop/Vita memory layouts and the reference renderer remain intact.

Validation completed so far:

- CTest: 25 passed, two optional Lua tests skipped (lupa unavailable).
- Full host build and synthetic paging, sample-reader, controls, audio queue
  and lifetime tests pass; paging/sample-reader/audio sanitizer runs pass.
- Build and private-install helper tests pass without any game data.
- 6,000-frame dense/paged host race: 196,665,345 i960 and 223,429,779 TGP
  instructions, identical geometry, board/video memory and sound commands.
  Both boards in this test use sparse tables and the exact PSP render flags.
- A visible 240-frame attract window (1440-1679) matches every CPU-rendered
  pixel and hash, digest `c49a1f7f0e07f872`. All 1,680 attract frames also
  match; final `c24dbbad1b5f898f` matches the ordinary desktop m2run, with
  52,998,145 i960 / 46,157,202 TGP instructions on both builds.
- The shared changes also rebuild and package for Vita with logging OFF;
  the archived GPU25 release was not overwritten. No new Vita hardware test.
- PSP cross-compilation, PBP/PRX imports and SFO MEMSIZE=0 pass. ROM preparation
  uses the existing CRC/SHA-checked importer into ignored private build output.
- Final isolated PPSSPPSDL 1.20.4 original-PSP (PSPModel=0) smoke completed
  600 frames with status=ok in 244.214 seconds (about 2.46 frames/s including
  startup). Boot settings and textured 3D attract scenes visibly rendered.
  Final framebuffer hash `36945e52a376dc48` matches desktop m2run at 600
  frames. Audio shutdown succeeds with zero invalid/unsupported/error results,
  but 453 audio blocks exceeded 10.667 ms (peak 96.472 ms) and the emulator
  logged underruns. This is NOT smooth playback or physical hardware proof.
  Final malloc arena used 18,083,336 bytes, free 773,624 bytes; kernel free
  memory was 1,306,624 bytes. Those different pools must not be conflated.
- Private install folder: `build/psp-test01/PSP/GAME/DAYTONA`, prepared with
  verified ROMs and no smoke files. EBOOT is 4,523,666 bytes, SHA256
  `911e4fbdbfc788794154e37244952918973ed7327cdfa5d00f479f07d67751ad`.
  PRX loaded size is 0x3c7370 bytes; full game-derived binaries remain ignored.

Findings, including rejected assumptions:

- A direct-mapped cache caused 3.623 GB of main-board reads over the host
  6,000-frame race. Four-way replacement plus a last-page shortcut reduced
  that to 1.461 GB with the same payload budget. It still may stall a real
  Memory Stick; audio reads are additional, and startup is included.
- PSPSDK's PBP helper treats numeric MEMSIZE=0 as missing and defaults to 1.
  The PSP build creates an explicit SFO to retain the original-PSP RAM limit.
- Runtime/C++ libraries linked after SDK import libraries produced unsafe
  out-of-order imports. Explicit runtime-first link ordering fixes the package.
- PSP int32_t is long, unlike the host's int. Explicit int32_t template
  arguments in raster/MultiPCM preserve arithmetic while making these compile.
- Stock PPSSPPHeadless forces the Slim model; use isolated PPSSPPSDL with
  PSPModel=0 for an original-PSP memory test, not a headless success claim.
- The first 600-frame emulator attempt did not finish before its 240-second
  timeout. A shorter test found SRC release returning 0x80268002 (channel still
  reserved), not the generic 0x80260002 output-busy code. This exposed a drain
  handling bug. Both known busy codes now retry for up to 250 ms; unknown
  errors and timeouts still fail. Optimized/ASan/UBSan/TSan regressions cover
  successful drain, timeout, unknown errors and retained output-buffer lifetime.
- The first 120-frame test had no active voices and no over-budget audio blocks
  (peak 1.031 ms); board wall time was about 117 ms/frame. Do not blame audio
  starvation or claim smooth PSP gameplay from this silent boot test.
- Presentation skipping only reduces GU uploads. It deliberately does not skip
  board instructions, geometry, sound commands or CPU rasterization.

Reproduce via `platform/psp/README.md` and `scripts/test_psp_memory.sh`.
Next: profile/replace the slow PSP CPU rendering and storage hot paths, then
validate the private install on an actual PSP-1000: full race, active audio, pause/resume/reset,
save persistence and shutdown. CPU rasterization and ROM I/O remain performance
risks; there is no verified smooth hardware gameplay result yet.

## Current state

**Draw distance (enhancement, off by default).** Launcher slider (Shortest,
Shorter, Default, Further, Furthest = -2..+2), `m2run --draw-distance N`.
Found by tracing, not by guessing: the geometrizer's master z clip is unused
(0xff); object commands in the display list are written by the TGP, which
reads model lists from its own ROM (only 8 of ~3,000 i960 FIFO words per
frame match model addresses). The i960 picks what to draw:
- Scenery by course cell: a 16x16 grid (cell = x + 16 y; offsets table
  0x17136 = dx + 16 dy). 0x16f74..0x17070 takes the 5x5 around the car's cell
  (r8) in nearest-first order (0x17104; 0x1711d on two courses), kept where
  two visibility masks allow (r13 from 0x171f8, r9 from 0x1727c), into the
  draw list (count 0x5016c0, cells 0x5016c1.., 63 bytes before 0x501700) and
  a near list of the 10 nearest (0x501600) plus bitmaps 0x501500/20/40.
  Measured over a race: 8-13 cells listed, always reaching radius 2.
- 0x17828 draws every object of each listed cell (table 0x501420) until a
  per-frame polygon budget runs out: 0x5010e8 accumulates each object's cost,
  0x5010f4 is the limit (5000, set once at 0x1210), checked at 0x17a78 and
  0x1786c. Extra cells appended to the list drew nothing until the budget
  was raised: it, not distance, is what stopped them.
- The road: a 14-section window (0x13f5c: 5 back via +0x8c, 14 forward via
  +0x88, list at +0x5c of its struct; ordered pair checks via table 0x13ec4
  and TGP maths at 0x14180). Also game logic (car/section interaction);
  not changed.
Mechanism: `m2recomp --hooks FILE` ("ADDRESS name": the generated code calls
rt::hook_<name>(c) before that instruction), `seeds/daytona93_hooks.txt`,
`src/runtime/enhance.{h,cpp}`. hook_draw_list at 0x17078: -1 keeps the
game's list within one cell, -2 the car's cell only; +1 lists all 5x5, +2
all 7x7 (inside the grid), and raises the budget to 10000 / 15000 (restored
to 5000 back at default). Only the draw list and the budget change; the near
list and bitmaps the game logic reads do not. Measured (race_basic, m2run on
this Mac): -2 242 frames/s, -1 198, default 193, +1 190, +2 185; default's
screen hash unchanged (ad67233983ea8808). +2 adds visible scenery (frame
2600: 1,560 pixels, a tree line behind the billboard); most frames differ
by tens of pixels, because the road, not the scenery, is the horizon.

**Setup's "ROM set rejected" was shown for any recompile failure.** A
Windows tester got it with a zip that imports on macOS and Linux. The
reader is portable C++ (binary I/O, fixed-width fields, its own inflate
and CRC), so the likelier cause is a later step failing on Windows (the
game's generated code has never been compiled there) under the wrong
message. `recompile.py` now exits 3 only when `m2import` rejects the set;
any other failure gets "ROM accepted, the build failed, see the errors
above". The tester's next run confirmed it: the ROM was accepted and the
generated code compiled (`m2run`, `m2native` built); the final build step
still failed. Likely cause, not yet confirmed on Windows: `daytona` is a
WIN32 (GUI) executable and `main.cpp` did not include `SDL3/SDL_main.h`,
so nothing provided `WinMain`. Now included (no effect on macOS/Linux).
The tester's next output showed the real failure: MSVC building SDL itself,
`yuv_rgb_internal.h` C2099 "initializer is not a constant". Our directory-
wide `/fp:strict` reached SDL, and under it MSVC will not fold C float
constants in static initializers. SDL now gets an empty COMPILE_OPTIONS
(ours keep /fp:strict). CI never built SDL (it was only added with
generated game code); SDL, ImGui and the app objects (`daytona_app`) now
build whenever extern/sdl3 exists, so CI compiles them on every OS; only
linking `daytona` still needs a ROM set. CI then compiled SDL, ImGui and
`daytona_app` under MSVC and clang-cl (including the yuv_rgb file that
failed). The tester's PC had built with MSVC: setup.ps1's Visual Studio
Installer `modify` ran unelevated and, it seems, failed quietly. setup.ps1
now runs it elevated, checks the Clang toolset with vswhere afterwards,
stops with instructions if it is missing, and sets M2_COMPILER=clang
(`--msvc` for MSVC). Untested on a real PC; CI only checks it parses
(GitHub's Windows runners have no winget). README's Setup and
docs/getting-started.md now cover Clang on Windows and `--msvc`, updating,
a clean rebuild, the "build failed" message, and the widescreen options.

**Widescreen (enhancement, off by default).** Launcher > Enhancements >
Widescreen: Original (4:3), 16:10 (614x384), 16:9 (682x384), 21:9
(896x384); `m2run --aspect 16:9` for headless dumps. More of the scene at
the sides, same focal length, nothing stretched: `Geo::set_wide_margin`
opens a full-width viewport's left/right clip planes by the margin,
`Raster` draws into a wider layer (stride >= 496 + 2 x margin) with the
clip widened for full-width viewports, `Video` composes at `width()` with
the tilemaps centred and each back-layer row carried into the margins (the
sky's colour, not the backdrop pen). With it off every path is the old one:
all 11 scenarios give their previous screen hashes. With 16:9 all 11 run to
the end. Sampled 21:9 race frames show no obvious edge pop-in yet; not
checked frame by frame.
"HUD at the screen edges" (with widescreen; `m2run --hud-edges`): two
groups in 496-wide coordinates move out by the margin: lap and lap times
(x < 125, y < 130) left; position, condition panel and course map
(x >= 352, y < 300) right. Decided per item: the front layers' pixels are
grouped into blobs (pixels within 4 of each other join) and a blob moves
only if it lies wholly inside a group, so a banner crossing a group stays
whole and centred. The first version decided per area (a band at the cut
had to be empty): during the rolling start the banner's letters passed the
cut, the decision flipped frame to frame and the HUD jumped back and forth.
Measured after, frames 2585-3200 every 5th: the course map is at the edge
in all but the first (HUD not yet drawn). "40TH/40" reaches x 367. The
condition panel's box and car are polygons in the main 3D window at sort z
0x600 (scenery there is above 18000); overlay polygons (z <= 0x0fff) inside
a group move with it. The side margins are the sky's plain colour (the back
layers' top-left pixel); carrying each row's edge out smeared the sky
picture's clouds and mountains. Off: all scenario hashes unchanged; 21:9
with it on: all scenarios run to the end. rules.md now lets enhancements change game logic.
Measured for draw distance: Daytona leaves the master z clip at 0xff
(0 polygons culled by distance over a 6,000-frame race); backface 4.2M,
behind the camera 1.1M, off-screen 1.4M. The limit is in the game's code.

**Windows: Clang by default; MSVC fixed.** MSVC failed on SoftFloat: the
CMake used SoftFloat's `build/Linux-x86_64-GCC/platform.h` everywhere, whose
`opts-GCC.h` needs `__int128`, `__builtin_clz` and GNU inline (C4235, C4013).
MSVC and clang-cl now get `cmake/softfloat-portable/platform.h` (no
`INLINE_LEVEL`, no builtins, no 128-bit type) and `__declspec(thread)` /
`thread_local` for its globals; `-DM2_SOFTFLOAT_PORTABLE=ON` forces that
header anywhere. Measured here with it forced: `test_fp` 0 mismatches,
race_basic, time_attack and course_expert screen hashes identical to the
GCC-header build. `setup.ps1` installs (or adds to an existing Visual
Studio) the Clang tools; `setup.py` builds with the ClangCL toolset when
present, MSVC otherwise (`M2_COMPILER=clang|msvc` forces one), and
reconfigures a build directory set up for the other.
`.github/workflows/build.yml` builds and tests both, without a ROM set,
alongside macOS and Linux (GCC, Clang).
Not yet run on a Windows PC here; a tester reports Clang + Ninja builds.
First CI run: SoftFloat compiled under both; both then stopped on
`tests/test_vita_gpu_memory.cpp`, `alignas(262144)` (C2345; clang-cl: 8192
bytes at most on Windows). That buffer is now aligned at run time, and the
nine Vita host tests are opt-in (`-DM2_VITA_TESTS=ON`, 21 tests) instead of
part of every desktop build (12 tests).

**PS Vita frontend merged (PR #3, `c3007c6`).** Desktop unchanged by it,
measured: all 11 scripted scenarios and 3,000 frames of attract give the
same instruction counts and screen hashes as `398eb4b`; 21/21 tests pass.
Native audio is opt-in (launcher checkbox). Its notes live in
`platform/vita/` (HANDOFF, THIRD_PARTY for SDL2); the Vita build itself is
untested here (needs VitaSDK).

**Setup for players.** `docs/getting-started.md` walks from nothing to
playing on each OS, with troubleshooting for what went wrong in practice:
`./setup` for `./setup.sh`, the ROM set under another name (setup silently
built tools only), a different Daytona set (`daytona`, program ROMs
`epr-16722a`/`16723a`: `m2import: missing epr-16530a.12`), Start disabled.
`scripts/setup.py` now says so itself: with no `roms/daytona93.*` it names
any archives in `roms/`; a rejected set gets an explanation instead of a
traceback; on success it prints the command that starts the game. README's
Setup links to the guide.

**macOS (Apple clang, arm64, Metal) builds and plays.** First run on a Mac
with a real `daytona93` set turned up:

- `scripts/recompile.py` built only the check tools after generating code,
  never `daytona` or `m2run`, so a fresh `./setup.sh` left no game. It now
  builds everything.
- The launcher saved the ROM path as typed (`roms/daytona93.7z`), so the
  check failed and Start stayed disabled when run from another directory.
  It now stores the absolute path. A saved GPU choice the host lacks
  (Vulkan on a Mac without MoltenVK) exited at `SDL_CreateGPUDevice`; it now
  falls back to automatic.
- The seeds were short. The windowed game stopped at `0x1d8c`, then longer
  runs at `0x2266f8`, `0x5788`, `0x225028`, `0x223078`, `0x2265c4`: code the
  game reaches only through pointers in states the harvest never visited.
  `scripts/seed_scan.py` finds them statically (see Findings) and added 248
  seeds (334 -> 582): 24,504 -> 34,497 reachable instructions, recompile
  still all native.
- Time attack stopped in the geometrizer: texture-point/header reads and a
  polygon-RAM walk run one past the end of their memory. `GeoPtr`/`GeoPtr16`
  now wrap (see Findings).

Measured after, `m2run`: attract 40,000 frames (11.6 min of game time),
all 11 scripted scenarios in `scripts/inputs/` (races, courses, time attack,
test menus) run to the end, all native; race_basic screen hash unchanged
(`ad67233983ea8808`) by the wrap. `daytona` on Metal: 60 s, 3,396 frames,
no stop. **Not verified against MAME**: the newly seeded code has not been
lockstepped (it only runs in states the scenarios reach, and the harvest
did not), and the wrap has no MAME counterpart to compare with.

**Sound.** The Model 1 sound board runs natively: its 68000 program
(`epr-16489`/`16490`) is statically recompiled (`src/m68k` decoder,
`tools/m2sndrecomp`, runtime context `src/runtime/snd_cpu.h`), with the
YM3438 (ymfm, BSD-3) and both MultiPCMs (MAME's, transplanted into
`src/runtime/multipcm.cpp`) as native code on `snd::SoundBoard`
(`src/runtime/sound_board.cpp`). All 1,916 reachable instructions decode
exactly as MAME's 68000 disassembler prints them; the driver has no
indirect jumps. Lockstep against MAME's 68000 (`tools/m2sndcheck`, MAME
patch 0003, `M2TRACE_SNDLOG`): attract 15.7M instructions and race 77.9M
instructions, 3,648 interrupts, 5.16M device accesses, all identical.
No clock: the driver polls YM timer B (868 Hz tick) in its main loop and
takes the UART's RxRDY on IPL 2, so time is counted in 68000 instructions
(752,000 per second, MAME's rate on this program) and events land on it:
command bytes one line-time apart (31.25 kbit/s), YM timer expiries in
exact YM clocks. `GameLoop` advances the board one frame per video frame
and hands it that frame's UART bytes. `daytona` plays it through two SDL
audio streams (YM at 55.6 kHz, MultiPCMs at 44.6 kHz) with a small speed
trim holding 60 ms of queue; volume and mute in the launcher. `m2run --wav
FILE` writes it headless. Against MAME's own audio (`-wavwrite`) for 26 s
of attract: the same 48 command bytes, per-second loudness within a few
percent, a constant ~125 ms offset (when the i960 sends the first
commands), no tempo drift. Race: 3,636 bytes natively vs MAME's 3,648.
Checked here with SDL's disk audio driver (no sound card in the
container): continuous output from the windowed game.

**Launcher.** `daytona` opens a Dear ImGui launcher in its window
(`src/app/launcher.cpp`): ROM browse (SDL3's native file dialog: Windows,
macOS; xdg-desktop-portal or zenity on Linux; typed path as fallback) with
per-file verification, graphics API and fullscreen; a Controls tab binding
every arcade control to a key and a gamepad button or axis half (press to
bind), analogue triggers for the pedals and a stick for steering, live
meters, dead zone, invert. Settings save to `launcher.ini` in the SDL pref
path as they change. The ROM set is loaded natively from the zip
(`src/runtime/zip.cpp`, own inflate; `rom_import.cpp`, the importer's table):
images byte-identical to `scripts/m2import.py`'s, in 0.8 s. 7z ROM sets too (`src/runtime/archive.cpp`: the
7-Zip LZMA SDK's public-domain decoder; solid LZMA2, LZMA, PPMd checked
byte-identical to the zip import, 0.9 s for 7-Zip's default). The build's
importer is now the same C++ code (`tools/m2import`, used by
`scripts/recompile.py`), so a .7z-only user can build. Esc in game
returns to the launcher (Resume, Reset, Quit). Verified here under Xvfb on
Vulkan: verification, the Controls tab, a key rebind saved to the ini, Start
from the zip, pause, and the zenity file dialog. Resolution and upscaling
options are to come.

**The game is playable in a window.** `daytona` (`src/app/main.cpp`): SDL
3.4.16 (built from source, static) with SDL_GPU: Vulkan or Direct3D 12 on
Windows, Vulkan on Linux, Metal on macOS (`--gpu` to choose). Each composed
frame is uploaded to a GPU texture and blitted, letterboxed 4:3, onto the
swapchain; the game advances at the board's 57.52 frames/s and is presented
at the display's rate. Keyboard and gamepad map to the I/O board; settings
EEPROM and backup RAM persist in the SDL pref path. Verified here on Vulkan
(Mesa lavapipe under Xvfb): the attract demo in the window. Direct3D 12 and
Metal not run yet (no Windows or Mac here). The 3D layer is still drawn by
the CPU reference rasterizer; the GPU rasterizer is a later step.
`rt::GameLoop` (`src/runtime/game_loop.cpp`) holds the frame pacing that
`m2run` and `daytona` share.

**The game runs on its own.** `m2run` runs the recompiled game on the
native board runtime (`src/runtime/m2_board.cpp`) with no trace and no
MAME: boot, the settings screen, then the attract demo in full 3D.
Headless for now (frames dumped every N; `scripts/rgb2png.py`); 600 frames
in 5.7 s including the CPU rasterizer (106 frames/s), 23 M i960 and 12 M TGP
instructions, all native.

How it runs, with no clock:
- Frame pacing is the game's: vblank starts when the game waits in its
  wait-for-vblank loops (0x12b0-0x12bb, 0x12f0-0x12ff: spinning on the
  frame counter at 0x00500000); a CPU-bound frame (the boot texture upload
  at 0x1388) gets vblank after one frame's worth of work (110k
  instructions, as MAME measures 25 MHz / 57.52 Hz). Vblank ends when the
  handler has returned to the wait loop. A windowed build waits for vsync
  there; frame rate is the only limit.
- The TGP runs before the i960 reads buffer RAM: the game sends the TGP a
  command, writes -1 to the mailbox at 0x0091fff0 and spins until the TGP
  writes 0 (0x1166c). Without this the game waits forever.
- I/O board: the dual-port RAM mailbox protocol, native (command 1: latch
  inputs into bytes 0-10; 3: load the 128-byte settings EEPROM into
  0x100-0x17f; 2: store it). Inputs take the scripts/inputs format
  (`m2run ... --inputs FILE`).
- Sound UART: TxRDY is immediate, so the IRQ3 handler drains its queue at
  once; the sound board receives the bytes at the line rate (Sound, above).

With scripted inputs (`--inputs scripts/inputs/race_basic.txt`) the whole
game flow runs standalone: coin-up, Circuit Select, car select, the race's
rolling start (3,000 frames in 23 s headless: 80 M i960 and 62 M TGP
instructions, 381 bytes sent to the sound board).

**The whole screen now renders natively, identical to MAME.** `src/runtime/video.cpp`
adds the segaic24 tilemap chip (four 64x64-tile layers, per-line scroll,
special window modes, 8-pixel window masks), the tilemap palette pens (as
MAME computes them at palette write time, refreshed each frame once a scroll
colour is written), the CRTC offsets, and MAME's composition order (2D back,
3D, 2D front). It runs at the i960 instruction count of each MAME screen
update (vblank end; patch 0002 `su`/`scr` lines) and is held to a hash of
MAME's composed screen: **596 of 596 attract frames and 5,996 of 5,996 race
frames identical**, 3D layer 5,587 of 5,587. Dumped frames (boot settings
screen, attract with HUD) are byte-identical to MAME's.

Everything on screen now comes from native code: the recompiled i960 and
TGP programs, and native C++ for the fixed-function chips (geometrizer,
rasterizer, tilemaps). Still replayed from MAME's trace in the harness: the
sound board (UART bytes) and the I/O board (dual-port RAM). Design change:
the sound 68000 will be statically recompiled, not interpreted.

**M3 started: the 3D layer renders natively, pixel-identical to MAME.**
`src/runtime/raster.cpp` is a CPU reference rasterizer (MAME's Model 2
renderer and the poly.h triangle/polygon setup, transplanted). In
`m2native`, at each vblank it draws the previous display list from our own
video memories (palette, colour translation, luma, texture RAM, all written
by the recompiled i960) and is held to a hash of MAME's 3D layer (patch 0002
`fb` lines): **423 of 423 rendered attract frames and 5,587 of 5,587 race
frames identical**, and a dumped frame is byte-identical to MAME's. A
mutant (one gamma entry off by one) fails every frame. Frame dumps:
`M2NATIVE_FBDUMP_DIR`/`M2NATIVE_FBDUMP_EVERY` (ours),
`M2TRACE_FBDUMP_DIR`/`M2TRACE_FBDUMP_EVERY` (MAME); `scripts/rgb2png.py`
converts either (game output: keep under traces/). Finding on the way: the
original Model 2's texture RAM keeps only 16 bits of each 32-bit write,
packing two writes per stored dword (MAME tex0_w/tex1_w); the bus now does
the same. CRTC offsets are still taken from MAME's log (they come from the
segaic24 tilemap chip, next).

**M2 geometry, native and matching MAME.** The whole geometry path now runs
natively inside `m2native`: recompiled i960, recompiled TGP, buffer RAM, and
the geometrizer (`src/runtime/geo.cpp`, MAME's HLE transplanted; the
original Model 2 geometrizer's DSP code is undumped). At each MAME vblank
(same i960 instruction count) the geometrizer walks our buffer RAM and is
held to MAME's log (patch 0002, `M2TRACE_GEOLOG`): every word it hands the
rasterizer and every polygon kept after culling and clipping, vertices bit
for bit.

| scenario | frames | rasterizer words | polygons kept | result |
| --- | --- | --- | --- | --- |
| attract | 597 | 11,954,823 | 403,475 | identical |
| race_steer_left | 5,997 | 151,874,210 | 6,658,999 | identical |
| time_attack | 5,997 | 136,744,927 | 5,937,107 | identical |

A mutant (luma off by one when it is exactly 100, in all four parsers)
diverges at frame 173, rasterizer word 385. A one-ulp change to a vertex
cannot show: the geometrizer hands the rasterizer 24-bit floats (MAME's
`f2u(x) >> 8`), which drop the low 8 bits. `scripts/m2_check.sh SCENARIO`
runs all of it (the race's geometrizer log is 2.8 GB of text; it is deleted
after the check unless M2_CHECK_KEEP is set).

**M2 started: the TGP program is statically recompiled and matches MAME.**
The TGP runs a 2,024-word program the i960 uploads from the data ROM;
`m2tgprecomp` turns it into native C++ (MAME's MB86233 semantics inlined per
instruction from `src/runtime/tgp.h`; no interpreter, no hand-written HLE),
and `m2tgpcheck` replays MAME's TGP-side log (patch 0002) through it:

| scenario | TGP instructions | input words | output words | banked accesses |
| --- | --- | --- | --- | --- |
| attract, 600 frames | 12,390,181 | 1,293,703 | 398,072 | 7,789 |
| race_steer_left | 230,600,970 | 21,489,825 | 6,727,552 | 3,233,496 |
| time_attack | 97,745,484 | 8,089,105 | 3,291,602 | 2,025,772 |
| test_tgp | 32,597,984 | 3,300,128 | 1,083,012 | 7,789 |

All identical to MAME; in attract every register was also checked after
every instruction (`M2TRACE_TGPPC`). A mutant (fml result off by one ulp when
A = 1.0) diverges at TGP instruction 2,339. Native speed ~500 M TGP
instructions/s (race: 0.48 s for 231 M).

**Native i960 + native TGP together** (`m2native` now models the geometry
ports, TGP FIFOs and buffer RAM instead of replaying them; the TGP runs on
demand, clockless, until its input FIFO is empty):

| scenario | i960 instructions | TGP instructions | TGP output words | buffer RAM hash |
| --- | --- | --- | --- | --- |
| attract | 70,926,456 | 12,390,181 | 398,072 identical | identical at 1,153 of 1,153 samples |
| race_steer_left | 643,000,979 | 230,600,970 | 6,727,552 identical | identical at 11,951 of 11,951 |
| time_attack | 663,975,129 | 97,745,385 | 3,291,602 identical | identical at 11,951 of 11,951 |

Buffer RAM (128 KB) is built natively from its three writers: the i960, the
geometrizer command port (0x800000) and the TGP's banked writes. The only
reads that differ from MAME (103 in attract, 64,956 of 5,451,850 in the race)
are all the i960 polling the TGP's mailbox, the last three dwords of buffer
RAM (0x91fff0-0x91fff8, TGP bank offsets 0x7ffc-0x7ffe): our TGP has already
finished when the i960 looks; MAME's, paced by cycle estimates, has not. A
timing artefact like the UART one, so lockstep uses MAME's value there and
counts it. Finding on the way: races read and write the TGP FIFOs 16 bits at
a time (MAME's 32-bit handlers see the whole dword, other lanes zero).

**M1 met with native code**: `m2recomp` statically recompiles 23,262
instructions (seeds: boot record + `seeds/daytona93.txt`) to portable C++,
and `m2native` runs them with no interpreter and no fallback. It matches
MAME for all 600 attract frames: 70,926,456 instructions, every one native,
1,153 samples, 3,315,201 device events, 627 interrupts; 1.3 s (55 M
instructions/s with lockstep checks on every instruction).
`scripts/recompile.sh` generates and builds it (into git-ignored
build/gen); `scripts/m2_check.sh` traces MAME and runs both harnesses.
An address with no recompiled code is a hard error naming it.

Beyond attract, the same native code matches MAME through seven scripted
scenarios (coin up, selects, races, test mode), every instruction, device
access and interrupt, including the sound-UART interrupts that land mid-code
during races:

| scenario | frames | instructions | device events | interrupts |
| --- | --- | --- | --- | --- |
| race_steer_left | 6,000 | 643,000,979 | 39,750,589 | 11,443 |
| course_advanced | 6,000 | 640,678,236 | 42,356,938 | 12,475 |
| course_expert | 6,000 | 641,018,396 | 42,010,671 | 12,419 |
| time_attack | 6,000 | 663,975,129 | 20,342,792 | 11,506 |
| test_mode | 4,000 | 439,587,066 | 6,954,614 | 4,051 |
| test_tgp | 3,500 | 385,086,539 | 6,708,822 | 3,578 |
| test_memory | 3,500 | 385,423,629 | 6,748,842 | 3,553 |

`scripts/m2_check.sh SCENARIO` reruns one (inputs from
`scripts/inputs/SCENARIO.txt`); each takes ~7 min of MAME plus ~15 s native.

The reference interpreter (`src/refcore`, MAME's executor) is a test
oracle only: `m2replay` uses it; the game build will never link it.

Earlier, **M1 reference milestone**: `m2replay` runs Daytona's own code through
the runtime (MAME's i960 semantics, transplanted) with devices replayed from a
MAME trace, and matches MAME for all 600 attract frames: 70,926,456
instructions, 1,153 samples (RAM hashes + registers), 3,315,201 device
events, 627 interrupts; 2.0 s.

M0 tooling runs against real MAME with the real game (`daytona93`, user's ROM
set, git-ignored `roms/` in this cloud container, never committed). Model-2-only
MAME with the harvest patch builds here (`scripts/build_mame.sh`, ~50 min cold,
15 s incremental); `scripts/run_trace.sh` runs it headless.
Built and tested here: i960 decoder + `i960dis`, trace library + `tracediff`,
and the MAME plugin `tools/mame-plugins/m2trace` (trace recorder, input
recorder/replayer). The plugin is tested only against a mock of MAME's Lua API.

Build: `./setup.sh` (Linux, macOS) or `setup.ps1` (Windows) installs the
toolchain, fetches the pinned dependencies, builds, recompiles the game if
`roms/daytona93.zip` is there, and runs the tests (see README.md). Verified
end to end on Linux with GCC and Clang; macOS and Windows (MSVC) not yet run.
The Lua tests need `pip install lupa` (Lua 5.4) and skip without it.

Running the plugin (user's machine, with their ROM set):

    M2TRACE_OUT=traces/attract.m2tr M2TRACE_FRAMES=600 \
    mame daytona -plugin m2trace -pluginspath "<mame>/plugins;<repo>/tools/mame-plugins"

`M2TRACE_RECORD_INPUT` / `M2TRACE_REPLAY_INPUT` record and replay inputs;
`docs/trace-format.md` has the rest.

## Complete

- Design document.
- Project rules (`rules.md`) and `.gitignore`.
- M0 step 1: "from memory" figures checked against MAME `sega/model2.cpp` at
  `dddd73680656e355bb2b5beecab1167c9f07bf81` and the Model 2 MiSTer core at
  `591e148e87d27e03d50cbf7318bf0b1d1328c4bf`. Design doc corrected in place
  (Target hardware summary, TGP HLE, Floating point, Audio, Open questions).
- M0 i960 disassembler: `src/i960` (decoder + MAME-syntax formatter),
  `tools/i960dis` (linear sweep; `--interleave` joins the ROM_LOAD32_WORD pair),
  `tests/test_decode` (66 hand-encoded checks), `tests/mame_oracle`
  (differential against MAME's own `i960dis.cpp`, compiled unmodified).
- Ghidra SLEIGH cross-check (`tests/ghidra_oracle.py`, third-party module
  mumbel/ghidra_i960 via pypcode, `scripts/fetch_ghidra_i960.sh`).
- `src/i960/reach` (recursive descent + boot-record seeds), `i960dis --follow`,
  `tests/test_reach` (19 checks).
- M0 trace format, `tracediff`, MAME plugin and input recorder
  (`docs/trace-format.md`, `src/trace`, `tools/tracediff`,
  `tools/mame-plugins/m2trace`), with `tests/test_trace` (30 checks),
  `tests/lua_core_test.py` (19) and `tests/lua_plugin_mock_test.py` (17).

## Next, in order

1. Draw distance for the road: the 14-section window (see Current state)
   is shared with game logic; extending only what is drawn needs the draw
   side of it separated. Then the game's own 4:3 object culling, if
   widescreen shows pop-in.
2. Run `daytona` on Windows (Direct3D 12 and Vulkan) and fix whatever MSVC
   rejects. macOS (Metal) is done (Current state).
   Harvest the states `seed_scan.py` found in MAME (which state the windowed
   game was in at `0x1d8c`, what reaches `0x2266f8`) and lockstep them.
3. GPU rasterizer for the 3D layer (SDL_GPU pipelines; shaders compiled to
   SPIR-V, DXIL and MSL), measured against the CPU reference.
4. Wheel support and control remapping; resolution options (widescreen is
   done).

## Open decisions

- Project licence. The Model 2 MiSTer core is GPL-3; lifting from it decides
  this.
- FP oracle for lockstep: MAME disagrees with the hardware model on cvtri
  ties (see Findings). When a replay diverges there, the trace diff will show
  MAME's value; the recompiled build follows the model (rules: PCB > MAME).
  Settling which the PCB does needs a hardware measurement.
- `addc` carry. MAME never sets it. Recompile to the silicon and flag the diff
  when it fires (the MiSTer core made the same call, its study §2.3).

## Findings

Confirmed from MAME `model2.cpp` (all MAME figures, not PCB measurements):

| Item | Was (from memory) | MAME |
| --- | --- | --- |
| i960KB clock | ~25 MHz | 25 MHz (`50_MHz_XTAL / 2`) |
| TGP | MB86234 + microcode ROM | one MB86234 at 50 MHz; program uploaded by the i960, 2,024 words from the data ROM; run LLE |
| Resolution | ~496x384 | 496x384 active, 656x424 total, 16 MHz pixel clock |
| Refresh | ~57.5 Hz | 57.524 Hz; line rate 24.39 kHz (MAME: "TODO: from System 24") |
| Sound | 68000 + 2x SCSP, ~11 MHz | **Model 1 sound board**: 68000 @ 10 MHz + YM3438 + 2x MultiPCM |
| Main to sound | command latch | i8251 UART at 31.25 kbit/s; IRQ3 handler is the transmit loop |
| I/O | direct ADCs | Model 1 I/O board, own Z80 @ 4 MHz, via MB8421 dual-port RAM |
| IRQ order | unknown | bit 0 vblank -> IRQ0, bits 2-5 timers -> IRQ2, bit 10 UART -> IRQ3; ICR 0f0e0d0c (measured): all priority 1 |

i960 decoder (MAME `i960.cpp` / `i960dis.cpp` at `dddd7368`):

- MAME's executor implements 62 non-REG and 102 REG opcodes; its disassembler
  knows many more (whole-family table). The recompiler accepts only the
  executor's set.
- Executor and disassembler disagree on three encodings; decoder follows the
  executor and flags them: CTRL/COBR bits 1:0 (executor adds them to the
  target), MEMB bits 6:5 (executor ignores), MEMB scale > 4 (executor shifts).
- MAME's disassembler table has `ldtime` at 0x671, shadowed by `ediv`; it can
  never print. `movre` appears at 0x6e1 (undocumented, executed) and 0x6e9.
- Differential test, random + every opcode byte x 65,536 tails: 71,108,864
  words, 0 mismatches, 8.0 s on 4 threads (this container). Mutation check:
  three deliberate faults (MEMB scale bound, COBR displacement mask, one REG
  mnemonic) gave 79,619 / 2,220,826 / 9,837 mismatches, so the test can fail.
- Exhaustive run, `mame_oracle --exhaustive`: all 4,294,967,296 first words
  (second word random per word), 0 mismatches, 596 s on 4 threads (this
  container). Text and reported length both compared.

Trace tooling:

- Every signal the design doc asks for except indirect branch targets is
  reachable from MAME's Lua API alone (write/read taps, `read_range`,
  `state[]`), so the plugin needs no MAME patch yet.
- MAME's end-of-frame notifier is not on a guest instruction boundary, so it
  cannot be a lockstep sample point; the default is the vblank-ack store.
  Unverified for Daytona until the first trace.
- Mock-driven plugin test: record -> replay reproduces the trace exactly; a
  replay value applied one frame late is caught at the right frame; a memory
  change is reported as a region hash at the right epoch.
- The Lua encoder and the C++ writer produce byte-identical traces for the
  same content; the Lua hash equals an independent Python FNV over 8 sizes
  either side of the 1 KiB unpack block.
- Hashing reads 1.4 MiB per sample in Lua; cost unmeasured until a real run.
  If it is too slow, hash fewer regions per sample, not a weaker hash.

First real MAME runs (`daytona93`, MAME `dddd7368` + harvest patch, 600
frames of attract, headless, empty NVRAM each run):

- Plugin loads and every tap fires: 3,567,333 events in 600 frames. 68 s per
  run with tracing on this container (MAME reports 14.9% speed).
- Found and fixed: `screen.frame_number` is a method at this MAME, not the
  property the Lua reference documents; and MAME silently drops errors raised
  in tap callbacks, so the first run had 0 samples and no message. Samples are
  now pcall-wrapped and failures reported.
- **MAME is deterministic for Daytona**: two independent runs gave
  byte-identical traces (60,942,182 bytes, 1,154 epochs, 3,567,333 events)
  and identical branch harvests.
- vblank-ack sampling works, but the handler writes the ack (`fffffffe`) twice
  back to back: steady state is exactly 2 samples per frame, the second epoch
  holding only the second ack. Deterministic, so lockstep is fine; every other
  sample is redundant (cost, not correctness).
- ICR = `0f0e0d0c`, set once (synmov at 0x00000a40): IRQ0-3 -> vectors
  0x0c-0x0f, **all priority 1**. Taken in attract: vector 0x0c (vblank) 576x,
  handler 0x0e00; vector 0x0f (sound UART) 51x, handler 0x0f50. Timers
  (IRQ2) never fire; final enable mask = vblank only.
- Geometrizer program port (0x00804000) **is** written: 411,757 writes, from
  epoch 89. TGP FIFO: 773,279 writes, 697,078 reads.
- Harvest: 16 `bx` sites / 49 targets, 2 `callx` sites / 56 targets; no
  `balx`, no `calls`.
- Static reach with the 107 harvested targets as seeds: 89 -> 13,081
  instructions, 0 stops on non-executable opcodes, **0 quirk encodings in
  reachable code**, 31 indirect sites (18 exercised by attract).

Scripted gameplay in real MAME (`scripts/inputs/race_basic.txt`: 3 coins,
start, confirm selects, hold accelerator; 6,000 frames, no steering):

- Replay self-check passes in real MAME: "replay matched the recording for
  5997 frames", in two independent runs; the two branch harvests are identical.
- First attempt stayed in attract: default settings take 3 coins per credit
  (screen showed CREDIT 1/3). With 3 coins, snapshots show car select, then
  the Beginner course, lap 2 of 8 by frame 6,000.
- Full run 103 s emulated at ~14.5% speed, with or without tracing (MAME's
  software 3D dominates; the Lua taps cost ~nothing).
- Race harvest: 20 bx sites / 53 targets, 6 callx sites / 113 targets.
  Interrupts over 6,000 frames: vblank 5,975, sound UART 4,820, timers never.
- Attract + race seeds (168): static reach 21,850 instructions (attract alone
  13,081), 26 of 35 static indirect sites exercised, still 0 non-executable
  opcodes and 0 quirk encodings reached.

Harvest over 15 scripted runs (attract, races, manual gearbox, time attack,
test mode; all replays self-checked "matched"):

| runs | merged seeds | static reach | indirect sites hit / found |
| attract | 107 | 13,081 | 18 / 31 |
| + race | 168 | 21,850 | 26 / 35 |
| + manual, time attack, test mode | 319 | 23,138 | 46 / 67 |
| + round 3 | 333 | 23,258 | 47 / 68 |

- Still 0 non-executable opcodes and 0 quirk encodings in reachable code.
- FP in reachable code: 108 instructions, only `cvtri` 39, `cmpr` 37, `cvtir`
  22, `scaler` 8, `cvtzri` 2. No FP arithmetic, transcendentals or extended
  forms: the FP oracle question shrinks to five operations.
- Reached and confirmed by snapshot: Beginner race (auto and manual with
  shifting), time attack (start + accelerator at car select), test mode menu
  and sound test.
- Not reached: Advanced/Expert courses (circuit select stays on Beginner with
  steering pulses held 30 frames at 0xe0 from frame 1560 and from 1450), TGP
  and memory test items (7 red presses from frame 1500 land on SOUND TEST
  twice, deterministically). Cause unknown; not guessed further.
- Circuit and car select confirm on an accelerator press ("step to choose");
  holding the accelerator from the start picks the defaults.

FP, step 1 (`src/i960/fp`, `tests/test_fp`, `tests/fp_vs_mame`):

- Operand forms measured: all 108 reachable FP instructions use g/l registers
  (single precision in and out); none uses fp0-fp3 or FP literals. AC =
  `3f001000` in all 1,153 attract samples: round to nearest, exceptions masked.
- Reference = SoftFloat 3e extF80; native fast paths = host float, round to
  nearest. Exhaustive proof: cvtri, cvtzri, cvtir over every 2^32 input, scaler
  over every 2^32 single at n = 0, 1, -1, 127, 128, -126, -127, -149, -150, 254,
  -300; 0 mismatches, ~5 min on 4 cores. Plus 36 hand-computed IEEE values,
  specials cross-product, 2^26 random each. A MAME-style fast cvtri
  (`std::round`) fails with 130,905 mismatches (quick run), so the proof bites.
- Bug found on the way: SoftFloat's `extFloat80_t` field order depends on
  `LITTLEENDIAN`, defined only in its private platform.h; C++ callers saw the
  other order and every result was wrong. Now a public definition.
- MAME vs model, every input (`fp_vs_mame`, ~15 min on 4 cores):
  | op | inputs | disagree (MAME on x86-64) | cause |
  | cvtri | 4,294,967,296 | 8,388,608 | every exact .5 tie: MAME `round()` away from zero, IEEE to even |
  | cvtzri | 4,294,967,296 | 0 | |
  | cvtir | 4,294,967,296 | 0 | |
  | cmpr | 268,435,456 pairs | 0 | |
  | scaler | 77,309,411,328 (18 exponents) | 14 | 0 x 2^n (n >= 1024), inf x 2^n (n <= -1075): MAME pow() gives NaN |
  MAME built for ARM64 also differs on 830,472,191 cvtzri and 830,472,191 +
  8,388,608 cvtri inputs (NaN, out of range): its C casts are UB and
  saturate, so MAME's own result is host-dependent there.
- Practical risk for lockstep: a Daytona cvtri on an exact .5 value. The
  trace diff will show it as a register/RAM divergence right after a cvtri.
- MAME never sets FP exception flags in AC; the model reports them. Where the
  i960 records them and whether Daytona reads them is unconfirmed.

M1 groundwork:

- UART-interrupt lockstep: option (a) chosen (safe points in the shipped
  build; a test harness replays MAME's delivery points).
- Delivery points are keyed by MAME's completed-instruction count (patch):
  a stalled FIFO op counts once. Attract, 600 frames: 70,926,456
  instructions, 1,882 interrupt events (1,254 line changes, 622 immediate
  takes, 5 pending-table takes). Two runs: identical IRQ logs and traces.
- Stalled accesses: MAME's `i960_stall()` rewinds IP to PIP, so the plugin
  marks an access with ip == pip as stalled (new record types 0x12/0x13);
  comparisons drop them by default.
- Read taps now cover every non-RAM range the i960 reads (irq, timers, geo,
  copro status to 0x3f, comm, renderer), so the harness can answer them all.
- `scripts/m2import.py` builds program.bin and main_data.bin from the user's
  zip (CRC-checked, MAME's layout) into git-ignored build/rom_cache.

M1 reference core (`src/runtime`, `tools/m2replay`, test only):

- Semantics: MAME's `i960.cpp` transplanted (BSD-3, notice kept); runs at
  36 M instructions/s interpreted.
- Bugs found on the way to MATCH, in order: (1) the bus treated
  read-only-tapped ranges as write-checked; (2) **MAME's ldl/ldt/ldq and
  stores advance the address only on regions flagged BURST** (RAM/ROM, geo
  program port, TGP function port, comm); elsewhere they repeat the address
  (FIFO pops). The bus now carries MAME's BURST flags per region; (3) **the
  plugin's region hash was wrong**: `read_range(first, last, 32)` steps one
  *byte* at a time, so it hashed a dword at every byte address. Fixed with
  step 4; MAME-vs-MAME comparisons had still passed because it was
  deterministic; (4) buffer RAM is written by the geometrizer data port and
  by the TGP (`copro_tgp_memory_w`), not only the i960, so in M1 it is
  treated as a device: the i960's accesses are recorded and replayed, and
  its hash is left to M2; (5) the plugin's own `read_range` fired the
  buffer-RAM read taps while hashing; taps are now suppressed while sampling.
- A per-instruction log on both sides (`M2TRACE_PCLOG`, `M2REPLAY_PCLOG`:
  count, PIP, AC, register-file hash) located bug (2) at instruction 109.
- Mutation check: addo off by one when src1 == 1 (fired 129 times) diverges
  at epoch 3; one program-ROM byte flipped (copied to RAM at boot) diverges at
  epoch 0. Three earlier mutants never fired and so proved nothing either way
  (operand 12345, base 0x00500000, a data-ROM byte attract never reads).

Real program image (`daytona93`, epr-16530a/16531a, counts only):

- Linear sweep of the 256 KiB image: 55,098 lines, 16,280 undecodable words,
  7,356 flagged quirks, 759 non-executable opcodes. Mostly data decoded as
  code; not meaningful as code statistics.
- Recursive descent from the boot record (`i960dis --follow`, with the
  0x00220000 mirror): reset IP 0x860, 0 interrupt handlers (the PRCB's table
  is in RAM), 4 system procedures; 89 reachable instructions, 1 indirect site,
  0 quirks. The reset path ends in `b .` idle loops. Static analysis cannot
  get past boot without harvested targets.
- Mainline Ghidra has never shipped an i960 module (checked HEAD `8e9a8e7a`,
  the last 40 release tags, and full history). The user pointed to the
  third-party mumbel/ghidra_i960 (Apache-2.0), now used.

Ghidra SLEIGH cross-check (mumbel/ghidra_i960 `727ef787` via pypcode 3.3.3):

- All 23,258 reachable Daytona instructions: 0 differences (validity,
  mnemonic, length, target, operands).
- 1,000,000 random words, 1.5 s per 100k: after normalising syntax (MAME omits
  a x1 index scale, prints negative displacements unsigned, prints mode-5 as an
  absolute address; Ghidra prints `disp (ip)`), 72 differences in 5 groups,
  all known: MAME's disassembler names the integer src1 of cvtir/cvtilr/
  scaler/scalerl as an FP register (executor `get_1_ri` and SLEIGH read an
  integer); and SLEIGH decodes `movre` only at 0x6e1, MAME also at 0x6e9.
- The cross-check found four more encoding classes MAME treats specially;
  now decoder quirks: `sfr` (s1/s2, COBR bit 0: Cx special-function
  registers, ignored by MAME), `literaldst` (literal destination: MAME
  fatalerror, so no longer executable), `fpliteral` (FP literal other than
  fp0-3/+0.0/+1.0: MAME reads 0.0), `testfields` (test* with non-zero unused
  fields). 0 of any quirk in reachable code.
- Mutation check: making MEMB mode 7 read a displacement gives 2,871
  differences, exit 1.

Also found:

- A separate geometrizer (0x00800000 / 0x00804000) walks the display list in
  buffer RAM at vblank. It is not the TGP. MAME's is HLE in host `float`.
- The SCSP figures in the MiSTer core's design study are for 2A-CRX in general;
  its README confirms Daytona's working sound is 68000 + FM + MultiPCM. Both
  sources agree with MAME.
- MAME `subc` carry was fixed upstream since MAME 0.289; `addc` was not
  (operands still `uint32_t` before widening, `i960.cpp` ~line 1359).
- The design doc's "15 kHz capture" was wrong for this timing; changed to
  24 kHz medium resolution, pending PCB confirmation.
- macOS (Apple clang, arm64) failed to link `test_fp`/`fp_vs_mame`: C++ sees
  SoftFloat's globals as `extern thread_local` and calls a TLS wrapper
  function that the C (`_Thread_local`) definitions never emit. Linux accepts
  it, which is why it went unnoticed. C++ now gets `__thread` (GCC/Clang) and
  MSVC keeps `thread_local`. Checked: macOS `./setup.sh` builds with 5/5 tests
  passing; Ubuntu 24.04 arm64 GCC 13 and Clang 18 build `test_fp` with 0
  mismatches. Windows is untested, but MSVC's definition did not change.
- The harvest's seeds covered only the states the scripted MAME runs
  visited. The game reaches code four other ways, all now scanned by
  `scripts/seed_scan.py` to a fixed point (recompiling each round):
  - game-mode table at `0x18cc`, 31 entries, `ld 0x18cc[g0*4]; callx`
    at `0x18bc`, index the mode byte at `0x5010a0` (harvest reached 9);
  - task state chains: each state stores the next state's address with
    `lda` (`0x1dd4`: `lda 0x2266f8,r5; st r5,0xc(r3)`; `0x2266f8` itself
    stores `0x226764` at `0xc(g13)`);
  - jump tables read as `ld T[r*4]` then `bx` (`0x5808`, masked to 4) or
    `lda T[r*4]` then `ld`, `callx` (`0x225010`);
  - handler addresses in ROM data: 24-byte object records in the program
    ROM (`0x2375bc`: `0x223078` then floats) and tables in the main data
    ROM (`0x28660cc` -> `0x2265c4`, a bare `ret`).
  A candidate counts only if `i960dis` decodes it to a `ret` or branch with
  no `?`, `!noexec` or `!quirk` first. That rejected float constants that
  `lda` loads (`0x50d0` = 1.0f, `0xcdd8`). From the committed seeds the scan
  finds 275 entry points in one round and nothing in the next; 248 of them
  are the added seeds, the other 27 fall inside code those reach.
- Geometrizer reads past the end of memory (time attack, frame < 6,000):
  texture point/header data (`GeoPtr16`, 4-8 words from a masked start) and
  a polygon RAM walk (`GeoPtr`, index 32,768 of 32,768). MAME's raw
  pointers read whatever follows the array there. The display list is not
  ours at fault: time attack matched MAME to the word, buffer RAM included.
  Now wrapped to the memory size, as a hardware address counter would;
  unconfirmed on the PCB.

## What not to re-propose

- ARK expanded RAM or SD swap as a PSP-1000 memory fix: its high-memory
  path explicitly excludes this model. File-backed ROM caching is different.
- A PSP compile or original-model emulator boot as smooth hardware proof.
  The initial 600-frame PSP smoke is only about 2.46 frames/s.
- Presentation skipping as a CPU-raster optimization: it only skips upload.

- SCSP for Daytona. It is the Model 1 sound board (MAME `model2o` config and
  the MiSTer core's working sound on hardware).
- Five TGPs. One device; "5x" was a board-level package count.
- MAME's frame notifier as the lockstep sample point (see Trace tooling).
- `read_range(a, b, 32)` without a step of 4 (reads every byte address).
- A mutation test whose mutant is not shown to fire.
- Using MAME's disassembler as the decode authority. It is the text oracle
  only; semantics come from the executor.
- `THREAD_LOCAL=thread_local` for C++ users of SoftFloat (breaks the macOS
  link; see Findings).
- Seeding one "no recompiled code at X" at a time. Each state stores the
  next, so the game stops again one state on; run `scripts/seed_scan.py`.
- A function-start test (previous word is `ret` or `b`) on its own for code
  pointers: it rejected `0x2266f8`, whose previous word is the second word
  of an 8-byte instruction. `seed_scan.py` uses it only for the data ROM.
- Every aligned word of the 32 MB data ROM as a candidate without that test:
  5,497 candidates, mostly chance matches in graphics data.
- A TGP microcode ROM dump. The program is uploaded at boot from the game's
  data ROM; dump TGP program RAM only after the upload, or it is zeros.

M1 native (`tools/m2recomp`, `tools/m2native`, `src/runtime/lockstep`):

- One label per instruction; operands, branch targets and FP fast paths
  resolved at recompile time; `goto` for direct transfers, a dispatch switch
  for indirect ones. Unknown instructions or FP operand forms stop the
  recompile (exit 1), so nothing is left to run at runtime.
- Lockstep (`src/runtime/lockstep`) applies MAME's interrupt lines and takes
  at the same completed-instruction counts; shared by both harnesses.
- **IAC 0x93 (reinitialise) is an indirect transfer**: boot reinitialises to
  0x924 through `synmovq`. The first native run stopped there (no code); the
  harvest patch now logs IAC targets and the seeds include it.
- Mutation check: the generator's addo template off by one when src1 == 1
  diverges at epoch 3.
