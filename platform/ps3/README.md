# PS3 bring-up (PSL1GHT)

This branch contains display/input and bounded game-runtime tests, not playable Daytona.
Other platform builds are unchanged. No proprietary Sony SDK is used.

## Build

```sh
python3 platform/ps3/build.py --sdk /usr/local/ps3dev
python3 platform/ps3/build.py --sdk /usr/local/ps3dev --check-cxx
```

Outputs stay in `build/ps3-current`: `daytona-ps3-probe.pkg`, `.self` and `.elf`.
The builder does not modify the SDK or deploy anything. The installed GCC 7.2
builds the GNU C11 test, but rejects the game's C++20 capability probe.
PSL1GHT syscall headers require GNU extensions (strict C11 fails on `asm`).

## Hardware test

On a PS3 already configured to run homebrew, install the PKG using its package
manager and launch **Daytona PS3 display test**. Title ID: `DAYTPS301`.
No ROM is required. Expect eight colour bars and a moving white strip.
Cross changes the lower area's background to green; Start+Select exits.
The PS/XMB exit callback is also handled. Test with one controller first.

Retrieve `/dev_hdd0/game/DAYTPS301/USRDIR/ps3-boot.log`; standalone SELF
launches fall back to `/dev_usb000/ps3-boot.log` if the installed path is absent.
If both paths fail, display/input still run without a log. Report colours,
movement, controller response and whether exit worked. A successful build or
120 submitted flips is not proof of visible output. Polling flips has a bounded
wait; driver calls themselves are not guaranteed to return on hardware failure.
Display memory remains owned until process exit rather than freeing scanout.

## Next stages

An isolated GCC 13.2 upgrade now builds and links the C++20 capability probe:

```sh
bash platform/ps3/upgrade_toolchain.sh
python3 platform/ps3/build.py --sdk build/ps3-sdk --check-cxx
python3 platform/ps3/build.py --sdk build/ps3-sdk --runtime-probe
```

The system GCC 7.2 SDK is untouched. Preserve `build/ps3-sdk` and
`build/ps3-toolchain-source` as rebuild dependencies. Upstream sources are pinned;
the two host compatibility adjustments disable optional MPFR float128 and keep
old newlib documentation-generator pointer/int conversion diagnostics as warnings.
Neither changes the target's FP semantics. Initial build may take several minutes.

The runtime package uses the same title ID and log as the display test. Before
display initialization it exercises vector/span, bit_cast, caught exceptions and
basic main-thread TLS; require the explicit C++ PASS line on hardware. It does
not validate worker-thread TLS, full game execution, audio or gameplay speed.

1. Validate this display/input/XMB-exit test on hardware.
2. Provision a separate compatible C++20 PPU compiler and standard library;
   validate exceptions/TLS and ABI rather than mixing host libraries into PS3.
3. Build the unchanged game runtime with explicit guest-endian access; compare
   startup hashes against the host. Xbox ABI/TLS shims are not reusable on PS3.
4. Present CPU-rendered frames through RSX display buffers, preserving pixel
   semantics. Add native audio and game controller mapping independently.
5. Measure memory and frame timing on hardware before optimization or SPU work.

Initial test uses two output-resolution XRGB buffers (about 16.6 MB at 1080p)
plus 1 MB main-memory command storage. Gameplay budgets remain unmeasured.
It uses no shaders and needs no Cg compiler. There is no 60 FPS gameplay claim.

## Validation recorded

Revision 9 hardware measurements: display ~11.3 ms in 3D, down from ~48 ms;
game/render ~95-135 ms. Revision 10 adds BREAKDOWN timing summaries every
60 frames using existing VideoProfile hooks. video_total includes tile cache,
tile draw, raster and composition; do not add those twice. geometry_vblank
also includes interrupt/communication work at vblank start; execution_other
includes CPU/TGP dispatch and callbacks. No optimization claims from these
counters until the next hardware log is collected.

Revision 8 measured ~48 ms presentation and 92-133 ms game/render in the 3D
section. Revision 9 caches scaling coordinates and reuses main-RAM output rows.
test_scale.cpp compares the original formula pixel-for-pixel under ASan/UBSan.
New PRESENT summaries separate wait/input, scale/copy and command submission.
Hardware performance and image confirmation for this optimization are pending.

Revision 7 passed all 600 startup frame hashes on physical PS3. Revision 8 is
a performance baseline (not a hash-validation test):

```
python3 platform/ps3/build.py --sdk build/ps3-sdk --performance
```

It removes state/frame hashing and verbose traces, logs mean/max game/render
and display microseconds every 60 frames, then exits after a bounded hold.
Display includes flip waiting. Retrieve ps3-boot.log to identify the bottleneck.
Audio and game controls remain unimplemented in this diagnostic frontend.

Revision 6 isolated the first difference to a clipping-plane normal, with
equal input vertices. Revision 7 replaces the PS3 compatibility shim's hypotf
call with double-precision square/sum/sqrt and float rounding. The host sweep
test_plane_math.cpp checks 524284 normalized components across integer planes
-65535..65535 against std::hypot. Target validation remains required; hashes
are not relaxed. The rest of the game's arithmetic is unchanged.

Revision 5 found differences before raster projection. Revision 6 adds GeoClip
input-plane/vertex and edge-intersection records during frame 181 only.
Use compare_clip.py with host-game.log and the downloaded log to locate the
first difference. Rendering behavior and strict comparison remain unchanged.

Revision 4 showed the selected texture coordinates differ before shading.
Revision 5 adds RasterGeometry stages 0 (source vertices), 1 (projected and
perspective parameters), and 2 (edge calculations) for the same three pixels.
compare_raster.py compares both these records and the pixel calculations.
This remains diagnostic, with the original arithmetic and strict hash check.

Revision 3 narrowed frame 181 to 22 differing video pixels: CPU registers,
main/work RAM and TGP instruction count match. Revision 4 captures selected
raster calculations in RasterPixel log records. Compare host-game.log with
the downloaded log using compare_raster.py. Rendering arithmetic is unchanged;
the diagnostic is compiled only for this PS3 bring-up and its host reference.

Display revision 2 reached frame 181, where its combined hash differed despite
matching instruction count, TGP count and IP. It logged FAIL and held the last
frame; the user's HUD photo confirms pixels reached scanout. Pad exit failed.
Revision 3 logs independent component hashes and saves USRDIR/frame181-state.bin.
It returns to XMB on failure and limits the success hold to about 15 seconds.
Compare downloaded state with the private host dump using compare_state.py.
The mismatch itself remains unresolved; do not treat this as a rendering fix.

The 120-frame real-runtime test passed on physical PS3 with every host hash
matching and clean XMB return (hardware-game.log). Next presentation test:

```
python3 platform/ps3/build.py --sdk build/ps3-sdk --game-display
```

It regenerates a 600-frame host reference, presents checked CPU pixels using
RSX display buffers, and holds the final frame until Start+Select or XMB exit.
Expect original 4:3 aspect with black side margins on widescreen displays.
No audio/gameplay input yet; startup presentation is not full-race validation.
--reuse-runtime-objects is only for frontend-only iterations with known matching
objects; omit after any runtime/header/compiler changes. Normal builds rebuild
everything. A successful build does not prove visible output on hardware.

Physical PS3 display, Cross press/release and clean exit passed. The isolated
GCC 13.2 C++20 vector/span/bit_cast, caught exception and main-thread TLS test
also passed (build/ps3-current/hardware-cxx.log).

Build the next bounded real-runtime test with:

```
python3 platform/ps3/build.py --sdk build/ps3-sdk --game-probe
```

This generates private sources and a fresh 120-frame host oracle in separate
ps3-host-tools output. It reuses the Xbox diagnostic's platform-neutral Runner,
not its TLS or exception shims. M2_XBOX360_HEADLESS currently names the shared
no-network diagnostic boundary. Copy the six build/ps3-current/roms/*.bin files
to /dev_hdd0/game/DAYTPS301/USRDIR/roms separately; they are not in the PKG.
Install the new PKG, launch, and retrieve USRDIR/ps3-boot.log. A blank screen
followed by XMB is expected. Require 120 matching frame hashes and final PASS.
This is not audio, displayed gameplay, timing or full-race parity validation.
Compilation uses four workers. For target-only compiler/header changes,
--reuse-prepared-game skips regeneration and reuses the existing oracle; never
use it after runtime, recompiler, seeds or ROM changes. The normal command above
always regenerates both sources and reference. Target builds use GNU++20 and
cxx_compat.h because this SDK does not export several existing C99 functions
into std; this shim is not used by PC or other console builds.

GCC 7.2 cross-compilation, SELF/PKG generation, ELF64 big-endian PowerPC header
inspection and package listing passed. Package contains only PARAM.SFO and
USRDIR/EBOOT.BIN, with no ROM assets. Physical game-runtime validation is pending.
