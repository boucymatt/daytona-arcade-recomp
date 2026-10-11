# Xbox 360 port: bring-up, not a playable build

Target: RGH Xbox 360 S (Trinity), launched from Aurora. This is an isolated
OpenXeChain target; desktop/mobile/Vita build definitions are unchanged.
No proprietary SDK is used. User-supplied Trinity logs demonstrate boot,
guest-endian helper checks, filesystem read-back and player-one input polling.
Controller hotplug and dashboard return are not established by those logs.

**Build result (2026-10-10):** boot and input probes compile/link/package with
the pinned OpenXeChain SDK plus the completion step below. Each XEX is 260 KiB.
PowerPC/32-bit/Xbox PE headers and XEX2 signatures were checked. The toolchain
emits `/align specified without /driver; image may not run`; this warning is
recorded, not dismissed as proof of a valid hardware launch. The C++ gate
still fails with `<vector> file not found` in the unextended SDK. The separate
restricted libc++ build below resolves headers/containers for a third probe,
not the full exception/TLS ABI. None of the probes runs Daytona.

## Third-stage C++ probe

```sh
python3 platform/xbox360/build_libcxx.py --source /tmp/daytona-openxechain --sdk build-xbox360-sdk --jobs 4
python3 platform/xbox360/build.py --sdk build-xbox360-sdk --cxx-probe
```

Copy **only** `build/xbox360-cxx/default.xex` into a new writable directory,
for example `Hdd1:\Homebrew\DaytonaCxx\default.xex`. Launch via Aurora.
No ROMs are needed, and no picture is drawn. The program is intended to return
to the dashboard after writing `xbox360-cxx.log` in that directory. Preserve
and send the log even if a check fails or the completion line is absent.

Acceptance: static constructor, aligned allocation/overflow, vector growth /
sort / span, heap string copy, RAII destruction, double sqrt, and the final
`C++ probe complete: PASS` line. The user's xbox360-cxx.log contains PASS for
every check and the final marker, demonstrating these on the target console.
Build output is 397312 bytes; package/PE checks do not establish execution.

This uses the pinned LLVM libc++ source without modifications. The isolated
library deliberately disables exceptions, RTTI, threading, localization and
filesystem; no global SDK configuration or shared game build is changed.
`probe_alloc.cpp` supplies probe-only allocation because Xbox newlib's
aligned_alloc references missing `_memalign_r`. It is single-threaded,
aborts on throwing-new allocation failure, and does not implement new-handler
semantics. Do not use it as a complete game allocator.

The Generic CMake configuration creates a duplicate libc++.a rule for an
excluded shared target; giving that target a distinct output name resolves
it. This compiler fork also produced undefined function symbols when libc++
was built with function sections; the isolated build disables function/data
sections. Use `-I` for libc++ before SDK `-isystem` C headers, not a later
`-isystem` that makes libc++ wrappers select the wrong C headers.

The full existing `probe_cpp.cpp`, compiled with these headers, still fails
to link on `__cxa_allocate_exception`, `__cxa_throw`, catch/RTTI and other ABI
symbols. Exceptions, unwinding, TLS and threads remain gates before the game.
No exception handling was removed from the shared runtime to bypass them.

## Fourth-stage native thread probe

```sh
python3 platform/xbox360/build.py --sdk build-xbox360-sdk --thread-probe
```

Copy `build/xbox360-thread/default.xex` to a separate writable folder such as
`Hdd1:\Homebrew\DaytonaThread\default.xex`, launch via Aurora and retrieve
`xbox360-thread.log`. No ROMs, picture or input are required. The program is
intended to return to the dashboard after completion. Each thread join has a
five-second timeout; retain a partial or FAIL log too.

Two native workers each make 16384 lock-free atomic increments, set different
values in the same kernel TLS slot, and repeatedly check their own values.
The main thread checks its value was preserved, both workers exited, handles
closed, and the counter totals 32768 before logging the completion marker.
Workers use no heap or stdio because the SDK's allocator is not thread-safe.
On timeout, the TLS slot is not freed under a still-running worker, and worker
contexts remain static until process teardown. This is not std::thread or
compiler-generated thread_local validation. The user's xbox360-thread.log
now reports both workers, main TLS isolation, exact atomic count, cleanup
and final completion PASS on Trinity.

Further compiler audit: normal exception-enabled probe objects contain no
unwind/exception-table sections; PPCCOFFMCAsmInfo does not select an exception
format. Adding libc++abi alone is therefore insufficient. Trying
`-fsjlj-exceptions` fails in instruction selection with
`Cannot select: ... EH_SJLJ_SETUP_DISPATCH`. Native `thread_local` emits
`calls@tprel` relative to r2 with a `.tls$` section; compatibility with the
console's TLS layout is not established. Do not run that unverified sequence
or substitute a shared global for TLS. Kernel TLS is tested separately first.

## Fifth-stage compiler TLS probe

The first TLS package crashed on the user's console without a log. A missing
64 KiB mapped zero-fill page was found in its XEX. The builder now materializes
PE zero-fill into a generated `package.exe` before SynthXEX runs, then checks
the complete mapped image extent. The replacement XEX is 397312 bytes
(393216-byte payload plus header). This repairs a verified packaging defect;
the replacement has since passed the user's hardware test. TLS code is unchanged.

The user's replacement xbox360-tls.log now reports all checks and final
completion PASS, including three constructors and three destructors. This
demonstrates the bounded adapter on Trinity with the repaired package.

### Sixth-stage exception probe (passed on Trinity)

The user's `xbox360-exception.log` reports typed catches, nested cleanup,
rethrow, catch-all and final completion PASS. This validates the isolated
single-threaded probe, not full game or multithreaded exception support.

`prepare_exceptions.py` applies narrowly scoped patches to the pinned external
LLVM checkout and refuses unrelated changes in those files. It replaces the
Xbox printer's XCOFF type-reference code with format-aware common emission,
flushes DWARF frames from the PowerPC COFF streamer, and enables Xbox COFF
symbol aliases in libunwind. The installed SDK remains
untouched; these experimental tools build into a separate directory.

```sh
python3 platform/xbox360/build_exceptions.py --source /tmp/daytona-openxechain --sdk build-xbox360-sdk --stage compiler
python3 platform/xbox360/build_exceptions.py --source /tmp/daytona-openxechain --sdk build-xbox360-sdk --stage runtime
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk
```

The initial exception experiment is single-threaded. `exception_cases.cpp`
tests typed catches, mismatched catch bypass, cleanup across two stack frames,
rethrow and catch-all. Its host test passes 1000 repetitions under ASan/UBSan;
this is not evidence of Xbox exception support. `exception_probe.cpp` closes
a start marker before throwing so a fatal unwind failure can leave a log.
Copy only `build/xbox360-exception/default.xex` to a new writable folder such
as `Hdd1:\Homebrew\DaytonaException\default.xex`, launch via Aurora, and send
`xbox360-exception.log`. No ROMs, picture or input are required. It is intended
to return to the dashboard. Keep a partial log if it crashes or fails.

The 659456-byte XEX passed image-extent validation. Its linked image has 23
CIEs and 654 live FDEs, with five zero-address records for discarded duplicate
COMDAT functions. Static checks cover frame bounds, absolute personality/LSDA
addresses, PowerPC CIE parameters and coverage of the probe/throw/unwind entry
points. They do not prove CFI execution or exception handling on hardware.
Twenty Python checks pass. The linker still emits its existing `/align`
warning. No game runtime uses these experimental libraries yet.

The newlib allocator has no `_memalign_r`. The exception-only allocation shim
supports verified natural alignments up to 16 bytes, refusing larger requests
or a misaligned native result rather than returning an invalid pointer. It is
not a general game allocator. The frame-boundary assembly brackets all emitted
records, including archives; a COFF `$ZZZ` suffix puts the terminator last.

### Rebuilding the passing TLS probe

```sh
python3 platform/xbox360/build.py --sdk build-xbox360-sdk --tls-probe
```

Copy `build/xbox360-tls/default.xex` to a new writable folder such as
`Hdd1:\Homebrew\DaytonaTls\default.xex`, launch via Aurora and send
`xbox360-tls.log`. No ROMs or input required; no picture is drawn. Intended to
return to the dashboard. Retain a partial log if it does not finish.

The C++ source uses real `thread_local` variables. Clang's `-femulated-tls`
option lowers accesses to our `__emutls_get_address`, which locates each
thread's storage through KeTls APIs. It does not emulate game instructions.
Generated assembly was checked for adapter calls and absence of r2-relative
`tprel` accesses / `.tls$` data. Dynamic TLS initialization and compiler-emitted
`__cxa_thread_atexit` registration are exercised as well.

This isolated adapter is deliberately bounded: four attached threads, 32 TLS
objects and 32 pending destructors per thread, 16 KiB per-thread storage,
maximum alignment 256 bytes. It aborts on invalid descriptors/exhaustion rather
than returning corrupt addresses. There is no heap allocation, module unloading
or exception support. Every participating thread must call cleanup before
termination; init/shutdown are serialized and shutdown requires all threads
detached. No standard-library thread backend or shared game path uses it yet.

The hardware test checks zero and constant initialization, 64-byte alignment,
two concurrent workers' isolation, distinct addresses, main-thread preservation,
and three dynamic constructors/destructors. A rendezvous ensures both worker
contexts exist before either is released. Waits are bounded and a timeout
keeps TLS alive until process teardown.

Host adapter tests passed 200 worker lifecycles, storage reuse, reverse-order
destructors, busy-shutdown rejection and restart under ASan/UBSan (leak checking
disabled for the sandbox). Invalid size/alignment tests terminate with SIGABRT.
The compiler-generated probe has also passed on the user's Trinity console.

Exception follow-up: `-fdwarf-exceptions -funwind-tables` crashes in
`PPCXbox360AsmPrinter::emitTTypeReference`; this Xbox implementation accesses
an XCOFF TOC section even though the output is PE/COFF. Together with the SjLj
lowering failure above, this requires compiler/unwinder work, not merely
linking libc++abi. The isolated sixth-stage build above addresses the observed
compiler gaps; the installed SDK is unchanged and the exception probe passed
on the user's console.

## Seventh-stage runtime services probe (passed on Trinity)

The user's `xbox360-runtime.log` reports all stages and completion PASS,
including the 320 memory cases and call depths 1..12.

```sh
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk --runtime-probe
```

This builds exception-enabled libc++ from the existing isolated runtime
configuration and links unchanged `src/runtime/cpu.cpp`, the bounded compiler
TLS adapter and synthetic tests. Copy only `build/xbox360-runtime/default.xex`
to a new writable folder such as `Hdd1:\Homebrew\DaytonaRuntime\default.xex`.
Launch through Aurora and send `xbox360-runtime.log`. No ROM, picture or input
is required; intended to return to the dashboard. Preserve partial logs.

Checks: i960 reset state; 320 aligned/unaligned byte-order/flags/access cases;
call depths 1..12 including register-cache spills and restores; compiler TLS;
the real runtime's formatted Fatal exception; and libc++ out_of_range. Log
checkpoints are closed after each stage. Single thread only; the synthetic
bus does not validate M2Board's memory mapping, SoftFloat or generated code.

Host expectations pass ASan/UBSan (leak checking disabled); 21 Python tests
pass. The 724992-byte package has validated full mapped extent and 715 live
unwind records, including coverage of the CPU services. Hardware execution
of this integrated probe passed. Existing platform sources/builds and
the installed SDK are unchanged. Next: endian-correct SoftFloat and board/
generated-code integration before framebuffer, audio or gameplay claims.

## Eighth-stage big-endian SoftFloat probe (passed on Trinity)

The user's `xbox360-fp.log` reports aggregate ABI PASS, 57353 checks, zero
known failures, matching checksum `8b69a75c2515064d`, and completion PASS.

```sh
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk --fp-probe
```

Copy only `build/xbox360-fp/default.xex` to a new writable Aurora folder, for
example `Hdd1:\Homebrew\DaytonaFp\default.xex`. Launch and send `xbox360-fp.log`.
No ROM, picture or controller input is required. Intended dashboard return;
keep a partial log if it fails or crashes.

The isolated CMake build uses portable SoftFloat primitives and exposes the
host's field order to C and C++ consistently. LITTLEENDIAN is absent on Xbox;
the shared root build is unchanged. SoftFloat TLS uses the passing bounded
adapter. A C-returned extF80 aggregate is checked before 57353 results/flags
are hashed in fixed byte order, covering nine known results, special values,
deterministic random inputs, four rounding modes and scale exponents -500..500.
The reference checksum is `8b69a75c2515064d`; host Release and ASan/UBSan builds
agree with zero known-result failures. Expected data is generated under build/.

The 724992-byte XEX passes mapped-image and unwind-table validation. Xbox
execution passed. This is cross-host reference arithmetic testing,
not exhaustive parity with MAME, Xbox fast-path approval or a game boot.
M2Board memcpy loads/stores and texture-word casts still need endian review.

## Ninth-stage real board memory probe (passed on Trinity)

The user's `xbox360-board.log` reports construction, ROM access, all 3520 RAM
cases, mirrors/lanes, device-fetch guard and final completion PASS.

```sh
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk --board-probe
```

Copy only `build/xbox360-board/default.xex` into a new writable folder such
as `Hdd1:\Homebrew\DaytonaBoard\default.xex`, launch through Aurora, and send
`xbox360-board.log`. No ROMs, picture or input. Intended dashboard return;
preserve a partial log if it fails.

This constructs the real M2Board/Video/Geo/TgpBoard using synthetic images.
It checks program/main ROM fetch and protection, 3520 RAM cases across 11
regions, unaligned and page-crossing accesses, tile mirrors, TGP data-word
conversion/masked buffer writes, packed texture writes and device-fetch faults.
Only the test links board_probe_guards.cpp: scheduled callbacks, link activity
and generated TGP execution throw if reached. These are not game implementations.
No vblank, generated instructions or rendered frame is exercised.

guest_endian.h preserves memcpy on little-endian hosts and converts words on
big-endian hosts. It is used by board fetch/read/write and TGP/geometry ROM
conversion. Texture-RAM word views in rendering remain a separate task.
The host board test passes ASan/UBSan; geometry regressions pass 100000 clipping
cases and 6000 point-batch states. All 21 Python tests pass. The 856064-byte
XEX has verified mapped extent and board-service unwind coverage; its actual
Xbox execution passed. Linker's existing /align warning remains.

## Tenth-stage CPU raster probe

```sh
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk --render-probe
```

Copy only `build/xbox360-render/default.xex` to a new writable folder such as
`Hdd1:\Homebrew\DaytonaRender\default.xex`, launch via Aurora, and send both
`xbox360-render.log` and `xbox360-raster.ppm`. The latter is the actual synthetic
CPU framebuffer saved by the Xbox, not a screenshot of GPU presentation.
No ROM, screen output or controller input is required; intended dashboard return.

Texture RAM reads in the scalar and optimized CPU paths now use guest_load.
The test checks 80000 packed texel samples against an independent byte oracle,
then 16 non-empty textured triangle/quad frames: checker shading, mirror flags,
both sheets, changed texture origins and perspective. Each frame hash is
compared against a generated host reference; the last image is saved as PPM.
Failures log expected and actual hashes. This is not game-derived imagery.

The host Release and ASan/UBSan runs agree, as does the host build of the
Vita-optimized raster path. Its existing 2-million-footprint / 8-million-sample
test passes too. The 790528-byte XEX passed image/unwind validation. The user's
Trinity log passes all 16 frames and 80000 texels; its PPM is byte-identical to
the host reference. No GPU display or full MAME rendering parity is claimed.

## Eleventh-stage real game startup probe

```sh
python3 platform/xbox360/build_exceptions.py --source /tmp/daytona-openxechain --sdk build-xbox360-sdk --stage linker
python3 platform/xbox360/check_branch_linker.py --sdk build-xbox360-sdk --linker build/xbox360-llvm-eh/bin/lld-link
python3 platform/xbox360/build_exception_probe.py --sdk build-xbox360-sdk --game-probe
```

Requires your `roms/daytona93.zip` or `.7z`. The normal importer validates the
archive, and the normal recompilers generate native i960/TGP code. Imported
assets, generated sources and binaries remain in ignored `build/`; do not
publish them. This is not the synthetic board probe: no board test guards are
linked. Uses real free-run Lockstep callbacks, interrupts, TGP, geometry and
CPU video. A bounded test scheduler follows GameLoop::probe's 1024-instruction
checks, 110000-frame/40000-vblank caps and 2048 minimum; it does not link the
sound engine. Each frame hashes registers, plain RAM, screen pixels and TGP
count in fixed byte order against a native-host run. Stops on the first
divergence, missing code, no-progress dispatch or frame instruction limit.
The diagnostic target also caps each TGP run at one million instructions,
throwing instead of silently continuing if the cap is exhausted.

Copy these into a **new writable Aurora folder**:

```text
DaytonaGameProbe/
  default.xex       <- build/xbox360-game/default.xex
  roms/            <- build/xbox360-game/roms/ (all six .bin files)
```

Launch `default.xex`, let it finish, and collect `xbox360-game.log` from that
folder. There is intentionally no on-screen output, audio or input in this
stage; a return to the dashboard is expected. It runs only 120 startup frames,
not an interactive game. ROM images are size/hash-checked before constructing
the board (about 75 MiB of files; working memory is higher). Do not mix assets
from Revision A or an older build.

`M2_XBOX360_HEADLESS` removes unavailable iostream trace loading and threaded
network waits from this isolated target only. Free-run scheduling and offline
board semantics are unchanged; trace-file or active link use throws, not a
silent fallback. The existing PC/mobile/Vita builds do not define it. C++
runtime/allocator remains single-threaded. No graphics, sound, multiplayer,
full-race parity or Xbox FP fast-path approval is implied by a startup pass.

The larger generated program exposed the pinned COFF linker's incorrect
`isInt<24>` check on a byte displacement. REL24's instruction field holds a
signed 24-bit *word* displacement; the valid byte range is signed 26-bit,
4-aligned (also how LLVM's ELF PowerPC linker checks it). The isolated linker
stage corrects this and rejects misalignment. No range check is suppressed,
and no installed SDK binary is overwritten. The test decodes actual linked
forward/backward branches over 9 MiB and also requires overflow and unaligned
targets to fail. The headless game uses this linker; smaller probes retain
the original SDK linker.

The initial XEX builds at 11214848 bytes with validated mapped extent and
76 CIEs/1126 live FDEs/52 discarded COMDAT records. The existing `/align`
warning remains. It has **not been run on Xbox hardware**. The host reference
reaches frame 119 at 11476993 i960 instructions and 443 TGP instructions;
signature `23c8b9fbbbcea1b2`. This covers early boot, not an entire race.
The complete host ASan/UBSan run has no diagnostics and matches all 120
Release signatures. These are host checks, not a substitute for the Xbox log.

Hardware update: the user launched from USB through Freestyle 3. ROM
verification and board construction completed, then the console shut down
after `Begin frame 0 ip=00000860`. No completed frame or hardware PASS.

### First-frame shutdown trace

Latest crash boundary: frame 3 `Before vblank_end`, count `0x94000`,
IP `0x53ec`. To add CPU video-composition checkpoints:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --trace-video
```

This rebuilds video.cpp as well as the probe runner, with trace hooks enabled
only for this mode. Build prerequisites include `xbox360-llvm-eh`,
`xbox360-exception-runtime`, `xbox360-host-tools` and `xbox360-game` under build/.
Do not delete those caches when removing old executable builds. Until the
command succeeds, the existing current/default.xex is the previous test.

All `build_game_trace.py` modes now reuse `build/xbox360-current/`; copy
`build/xbox360-current/default.xex` to the console. Older mode-specific output
paths below are historical and no longer used. `build/xbox360-game/` remains
the required baseline objects/ROM directory, not the current test executable.

Latest paced-startup hardware run crashes after three matching frames, at
`Begin frame 3`, before cleanup or exit. Next targeted diagnostic:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --trace-fourth-frame
```

Copy `build/xbox360-game-fourth-frame/default.xex` beside the existing `roms/`.
It retains earlier pacing, adds detailed frame-3 checkpoints to
`xbox360-game-trace.log`, and stops after four frames. If it completes, it
closes the log after the 30-second observation and waits without automatic
exit. No picture/audio is expected; leave through Guide after a few minutes.
This is crash localization, not a verified fix.

The paced two-frame log completed both hashes, cleanup, 30 seconds of hold,
and the final pre-return marker, but shutdown was still reported. Next test:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --paced-startup
```

Use `build/xbox360-game-paced-startup/default.xex` with the existing `roms/`.
It checks 120 frames and writes `xbox360-game-trace.log`, then closes the log
and stays in kernel sleeps instead of automatically returning to the dashboard.
No picture/audio is expected. Allow several minutes, then use Guide to leave.
The final marker is `Parking without CRT exit; closing log; leave through Guide`.
If the console still shuts down, the last line locates the next investigation;
this build does not claim the shutdown is fixed.

Latest quiet-slice run still shuts down during frame 1. The next diagnostic is:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --paced-second-frame
```

Copy `build/xbox360-game-paced-slice/default.xex` beside the existing USB
`roms/` folder. It writes `xbox360-game-trace.log`, uses 1 ms native delays
between frame-1 slices without checkpoint writes, and holds for 30 seconds
after cleanup with one heartbeat per second before requesting dashboard
return through the CRT. No picture or audio is expected. A complete PASS log
does not establish a safe exit; inspect the observation and return markers.
This remains a diagnostic, not a shutdown fix or gameplay build.

The two-frame traced hardware test now passes both hashes and cleanup. Next,
keep its instruction slicing but disable checkpoint writes:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --quiet-second-frame
```

Copy `build/xbox360-game-quiet-slice/default.xex` to the USB game folder,
keep `roms`, launch through Freestyle 3, and collect `xbox360-game-trace.log`.
Its mode line must say `quiet second-frame slicing`. This comparison helps
separate slicing from logging effects; changing code generation is also a
possible confound. It is still a two-frame, no-display/no-audio diagnostic.

Latest hardware result: the 120-frame test passes frame 0 then shuts down
after `Begin frame 1 ip=000016b4`. Use the two-frame diagnostic next:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --second-frame
```

Copy `build/xbox360-game-second-frame/default.xex` beside the existing `roms`
folder on USB, run through Freestyle 3, and collect `xbox360-game-trace.log`.
The startup line says **2 frames**. Frame 0 is unchanged/unsliced; frame 1
gets instruction and phase checkpoints. This is not a confirmed crash fix.

Hardware update (2026-10-11): revision 2 passed the first-frame signature,
reporting, runner destruction, TLS cleanup and final PASS under Freestyle 3
from USB, with no fatal error reported. The next test restores the full startup
window with corrected logging, without instruction slicing:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --startup
```

Copy `build/xbox360-game-startup/default.xex` beside the existing `roms`
folder, launch through Freestyle 3, and collect `xbox360-game.log`. It runs
120 frames, compares each signature and returns to the dashboard. No picture
or audio is implemented. A host pass does not establish a console pass.

Revision 2 replaces the incompatible 64-bit printf arguments with pairs of
32-bit hex words. The original trace's counts matched phase-string addresses,
so those counts and IP fields are not trustworthy. The result line's trailing
string argument may also have been misread; no hardware fix is claimed yet.
New markers bracket each signature component, result formatting and cleanup.
The startup line identifies revision 2; replace the XEX, retain the same ROMs.

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk
```

Requires the existing `build/xbox360-game` objects and generated sources from
the preceding stage. This deliberately keeps those runtime/generated objects
and replaces only the runner, recording the baseline hashes in `baseline.json`.
It is not an incremental production build. A host ASan/UBSan test must match
the unsliced first-frame signature before the Xbox image is packaged.

Copy `build/xbox360-game-trace/default.xex` into the same USB game folder,
keeping its existing `roms` subfolder, and launch through Freestyle 3. Save
the previous XEX under a different name if wanted. Collect
`xbox360-game-trace.log` from beside it. No display/audio is implemented.
This test runs one frame only, tracing the first 64 instructions individually
and later batches of up to 1024 instructions. Logs bracket address lookup,
generated dispatch, vblank/geometry/video and frame hashing. All checkpoints
are flushed. Successful completion should return to the dashboard; another
shutdown is a failure, and the last checkpoint narrows where it happened.
Instrumentation does not guarantee that the shutdown is prevented.

Host trace passes ASan/UBSan and matches the original first-frame hash
`ea78bb6a56c2fbc5` at 151553 instructions. Leak checking is disabled due to
the sandbox's ptrace restriction. XEX mapped extent and unwind validation
pass (76 CIEs / 1127 live FDEs); console execution remains untested.

The diagnostic guard regression can also be run on the host:

```sh
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -DM2_XBOX360_HEADLESS -I src platform/xbox360/test_headless.cpp src/runtime/lockstep.cpp src/runtime/cpu.cpp src/runtime/comm_board.cpp -o /tmp/daytona-headless-test
ASAN_OPTIONS=detect_leaks=0 /tmp/daytona-headless-test
```

Sources: [OpenXeChain buildscript](https://github.com/OpenXeChain/buildscript),
[xecorelib](https://github.com/OpenXeChain/xecorelib),
[newlib](https://github.com/OpenXeChain/newlib), and
[SynthXEX](https://github.com/OpenXeChain/SynthXEX). Capability statements below
refer to the pinned revisions, not hypothetical support in other SDKs.

## Repository and SDK audit / implementation order

The audit covered root CMake/dependency setup, `src/app`, `src/runtime`, the
i960/TGP/sound recompiler interfaces, tests, CI and Vita/Dreamcast/mobile
frontends. This is an architecture audit, not exhaustive line-by-line parity
validation. Follow the design document's Architecture, Audio and Platform
sections: compile game code on the host, keep native platform services separate,
retain strict floating point, and never commit ROM-derived output.

OpenXeChain buildscript is pinned to `eed1fa65bf9577fd31625764b320a90182ea9ade`.
Its patched Clang/LLD emits PowerPC PE and SynthXEX packages it into XEX2.
Its `ppc32-xbox360` ABI is not interchangeable with a generic Linux PowerPC
compiler or libxenon ELF. It supplies newlib and kernel/XAM import stubs.

| Area | Evidence / missing implementation | Next gate |
| --- | --- | --- |
| Boot | Newlib CRT calls `main`, exit relaunches the dashboard | Build smoke XEX, launch through Aurora, retrieve marker |
| C++20 | Buildscript does not build libc++/libstdc++, C++ ABI or unwinder; Clang's C++ standard-library hooks are empty | Port/test containers, exceptions, constructors, math and TLS before linking runtime |
| Endianness | Root CMake and portable SoftFloat header force LITTLEENDIAN; board loads/stores use host memcpy; TGP ROM words use memcpy; CPU fast register spills and texture casts bypass conversion | Explicit guest-LE loads/stores; matching SoftFloat layout; actual big-endian parity tests |
| Rendering | SDL_GPU has no Xbox 360 backend; xecorelib has low-level Vd declarations, some with unknown arguments, not a renderer | Verified native framebuffer/present path first; upload CPU reference output; GPU acceleration later |
| Input | XamInputGetState exists in import .def; a separate probe supplies its signature/packet layout using Xenia's public ABI reference | Hardware-test slots, disconnect/reconnect and ADC mapping before game integration |
| Audio | Kernel audio import names exist, not a usable typed PCM backend | Verify submission ABI; bounded device-clocked queue, underrun counters, safe shutdown |
| Filesystem | Newlib includes Xbox syscalls and XAM file imports | Test game-relative read/write/close via marker before ROM/save handling |
| Memory | 512 MiB shared is a ceiling, not a free heap guarantee | Measure available heap/peak allocations; no full-image duplicate staging |
| Packaging | SynthXEX is explicitly early development | Validate XEX structure, imports and real Aurora launch independently |

Milestones, in order:

1. ROM-free boot/filesystem/endian smoke probe. A black screen during this
   probe is expected; there is no graphics initialization yet. It writes
   `game:\xbox360-boot.log`, delays briefly, then asks XAM to return to the
   dashboard. A built file is NOT proof of a successful launch.
2. Supply a working C++20 runtime and test its ABI, allocation, exceptions and
   SoftFloat on PowerPC. Build host recompilers separately from console code.
3. Convert guest memory boundaries (not every host integer!) to explicit
   little-endian accesses; test mixed byte/halfword/word operations, unaligned
   reads, register spills, TGP tables, palette/texture reads and save formats.
4. Establish display/input/audio independently using documented open APIs.
   Never call guessed Vd or XAudio signatures, or access GPU registers to
   compensate for missing APIs. Initial display should present the existing
   CPU raster output, without changing pixel semantics or adding shader forks.
5. Integrate GameLoop, ROM validation and native sound. Compare deterministic
   frame/input/audio evidence for both revisions with host reference results.
6. Measure frame pacing, memory high-water marks, audio underruns and thermal
   stability on Trinity. Only then optimize and consider release packaging.

Provisional memory envelope (budgets, NOT measurements): 128 MiB ROM/assets,
128 MiB runtime/geometry, 32 MiB frame/presentation storage, 8 MiB audio,
32 MiB code/stacks, with the remaining 184 MiB reserved for OS/driver and
headroom. Reject allocations gracefully; revise this budget from telemetry.

## Build the toolchain

Use Linux with Clang, CMake, Ninja, Make, Git, binutils, Python/PyYAML,
gawk, bzip2, gzip, zip/unzip and zlib development headers. Allow substantial
disk/RAM/time for LLVM. The upstream script cleans its own `build` directory:
run it only in a dedicated checkout, never in the Daytona source directory.

```sh
git clone https://github.com/OpenXeChain/buildscript.git /tmp/openxechain
git -C /tmp/openxechain checkout eed1fa65bf9577fd31625764b320a90182ea9ade
git -C /tmp/openxechain -c url.https://github.com/.insteadOf=git@github.com: submodule update --init --depth 1
cd /tmp/openxechain
cmake -S llvm/llvm -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=PowerPC \
  '-DLLVM_ENABLE_PROJECTS=lld;clang' \
  -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_TESTS=OFF
PREFIX=/absolute/path/to/openxechain-sdk PARALLEL=4 bash build-toolchain.sh
```

The initial configure seeds two cache settings the upstream script preserves:
optional LLVM benchmarks/tests are excluded. On this host, Clang 23 rejects
the bundled benchmark's `__COUNTER__` use under its warnings-as-errors policy.
This workaround does not patch compiler or target-runtime sources.

On a host with another LLVM installed, the upstream compiler-rt configure can
fail with `ADD_LIBRARY called with SHARED option` after discovering the host
LLVM package. Once the upstream Clang/newlib stages have succeeded, complete
the remaining C runtime and packager stages from the Daytona checkout:

```sh
python3 platform/xbox360/finish_toolchain.py \
  --source /tmp/openxechain --sdk /absolute/path/to/openxechain-sdk
```

This uses the pinned sources, disables host LLVM package discovery via
`LLVM_RUNTIMES_BUILD`, supplies the source-tree CMake modules, uses static
compile checks and installs only the target builtins and host packager.
It extends the SDK's generated Clang configuration with compiler-rt linkage,
idempotently. It does not supply libc++ or claim C++ support. This completion
path was built and tested locally; the GitHub workflow has not been run yet.

From the Daytona checkout:

```sh
python3 platform/xbox360/build.py --sdk /absolute/path/to/openxechain-sdk
```

`--check-cxx` runs a separate C++20 vector/span/exception/TLS link probe. It is
expected to fail with the unextended pinned SDK and must not be worked around
by using host C++ headers/libraries. A link pass alone does not validate ABI
behavior on the console.

Output is isolated in `build/xbox360-smoke/`. No ROM is required. The builder
does not pull Git, install packages, modify another platform or deploy files.
Keep compiler/packager output when reporting failures. Standard host Clang
cannot substitute for OpenXeChain's patched compiler.

## Deployment and acceptance (requires real hardware)

Physical framebuffer prerequisite:
`python3 platform/xbox360/build.py --sdk build-xbox360-sdk --framebuffer-probe`.
Writes the current XEX folder and logs to `xbox360-framebuffer.log`. It tests
an owned 640x480x4 physical allocation, page continuity, byte-pattern readback
and free, without GPU commands. Blank screen remains expected; return through
Guide after about ten seconds. Require the buffer PASS and parking markers.
This does not display the pattern or validate a GPU format.

Current display prerequisite (ROM-free):
`python3 platform/xbox360/build.py --sdk build-xbox360-sdk --display-probe`.
Output is `build/xbox360-current/default.xex`; it only queries display
information, writes `xbox360-display.log`, and parks on a blank screen.
Return through Guide. No GPU state is changed and no image is presented.
Hardware output guards and dimensions must be checked before implementing
native presentation; SDK video prototypes are incomplete.

The current longer startup diagnostic is built with:

```sh
python3 platform/xbox360/build_game_trace.py --sdk build-xbox360-sdk --paced-startup --small-draw-stack
```

This reuses the existing baseline objects and writes `build/xbox360-current/default.xex`.
It retains the heap tile mask that passed the four-frame hardware test, checks
120 startup frame hashes, and writes `xbox360-game-trace.log`. It draws nothing:
after cleanup and 30 observation heartbeats it deliberately parks on a blank
screen. Return through Guide to retrieve the log. A blank screen is not a pass;
require all reference hashes and the final parking marker. This longer test
still requires hardware validation and does not explain the earlier red light.

Copy the smoke `default.xex` into a separate writable directory such as
`Hdd1:\Homebrew\DaytonaSmoke\`; launch it via Aurora's file manager.
Do not replace your dashboard/default launch paths or write NAND/flash.
Retrieve `xbox360-boot.log` from that directory after it returns. Confirm the
marker reports both endian and filesystem write/read-back tests passing. If no marker is written, neither
boot nor filesystem support is proven. Debug output is supplementary only.
Record dashboard/kernel, Aurora and RGH versions, launch medium, behavior and
the XEX SHA-256. A freeze requires investigation before any game integration.

The probe intentionally does not use controllers, graphics or audio. Those
are open gameplay milestones. Full gameplay is blocked on
the C++ runtime and verified console presentation/audio interfaces above.

## Optional second-stage controller probe

After the basic boot marker works, build `--input-probe`. This produces a
different `build/xbox360-input/default.xex`; deploy into a separate directory.
It samples all four controller slots for about eight seconds, logs changed
packets/status to `xbox360-input.log`, then returns to the dashboard. Move the
left stick/D-pad and both triggers during the test. No screen is drawn.
Nonzero API status is logged with neutral controls (it must not be treated
as a connected pad). The adapter's byte layout and ADC mapping are host-tested;
actual polling, button bits and hotplug still require the console. The current
probe logs buttons but does not yet bind them to a game instance.

The XAM signature and 16-byte big-endian state layout were checked against
Xenia at the revision listed in THIRD_PARTY.md. No emulator implementation or
proprietary SDK was copied. This research is not hardware validation.
