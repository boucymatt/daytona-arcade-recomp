# Handoff

## PS3 span reuse and obsolete metadata uploads (2026-10-12)

Revision 53 completes 14100 logged frames and normal exit; user confirms correct
HUD/background. Revision 54 caches span geometry against the exact 12288-word
scroll/window snapshot, independently of tile and palette changes. Identical
snapshots reuse existing RSX vertices. Also removes obsolete per-frame register
and full palette uploads: the colour shader now only reads pen zero there.
Pen zero updates when the palette changes, while coloured tile invalidation
remains unchanged. RSX_LAYERS now reports span_rebuilds. This targets measured
3.4 ms layer work on unchanged frames; hardware benefit not yet established.
The single-return shader remains untouched. Audio is still absent.

## PS3 coloured-tile shader timeout isolation (2026-10-12)

Revision 52 hardware log records flip timeout before 60 frames. Revision 53
changes only the layer shader's backdrop selection from an early return to
two samples plus lerp/step and one return, leaving the coloured atlas allocation,
uploads, spans and renderer ordering unchanged. Earlier revision 45 also timed
out with a combined early-return shader, but correlation is not a diagnosis.
This controlled test checks that hypothesis; no crash fix claimed until tested.
Audio remains absent and the larger atlas/upload tradeoff is still unmeasured.

## PS3 coloured tile cache (2026-10-12)

User confirms revision 51 background/HUD appearance. Revision 52 expands tile
pens into cached RGBA variants: transparent category 0/1, opaque category 0,
and opaque any category. The fragment shader now samples colour and discards
zero alpha, removing per-pixel pen decode/category tests/dependent palette
lookup. Palette changes invalidate all cached rows; tile changes invalidate
their row bands. Atlas increases from 4 to 16 MiB RSX memory. This may increase
upload cost during palette changes; measure before claiming a win. Synthetic
C tests exhaust every pen/category/variant, including opaque black. No change
to span selection or 3D shaders. Hardware performance remains unverified;
audio remains absent.

## PS3 batched tile spans (2026-10-12)

Revision 50 regressed to 11.1 FPS: tile uploads are 0.14-0.31 ms but flip waits
82-86 ms even before geometry. The full-screen register-decoding shader is too
expensive in this test. Revision 51 moves scroll/window/split selection to CPU
span preparation, merges contiguous/vertical regions, and submits all background
rectangles then all foreground rectangles in two batched draws around 3D.
The fragment shader only samples indices, checks category/transparency and
looks up palette color. No CPU tile pixel composition. Capacity is 32768
rectangles with explicit failure on overflow, about 15 MiB CPU staging and
15 MiB additional RSX vertices. Source coordinates split at texture wrapping.
Synthetic coverage tests verify normal-mode scroll/wrap and no overlaps/gaps;
split-mode arithmetic tests remain. Hardware appearance/performance unverified.
Audio remains absent. Do not reuse revision 50 full-screen shader as a fast path.

## PS3 GPU System-24 composition (2026-10-12)

Revision 50 switches the PS3 runtime to desktop external-3D snapshots, bypassing
CPU tile composition. RSX layers.fcg adapts the shared m2.hlsl layer selection,
scroll, split, category, transparency and window-mask rules using float integer
arithmetic and texture-backed tables. Four decoded pixmaps occupy a 512x2048
RGBA atlas; changed tile-row generations select uploads. A 256x64 texture holds
4096 pens and 12288 snapshot words. Back and front are separate draws with the
existing geometry between them. Palette/register data updates every frame;
tile pixels only when dirty. Isolated layer/material programs remain separate.
Synthetic tests compare float split/wrap/mask arithmetic to integer rules;
this is not pixel parity or a hardware speed result. Must verify menus, HUD,
sky, scrolling and frame timings on PS3. Audio and exact 3D shading still open.

## PS3 layer comparison and static-quad cost (2026-10-12)

Revision 48 reaches 10620 logged frames and normal exit; user confirms HUD and
background correctness. Composition-copy timing is zero. Late layer upload
windows remain 4.3-4.7 ms. Revision 49 replaces memcmp with aligned 32-bit pixel
comparisons and initializes the constant layer quads only once, instead of
rewriting them in RSX memory every frame. No shader or composition changes.
A host C test checks equality and every individual bit in each of 496 pixels.
This is an optimisation experiment, not a measured speed gain until hardware
testing. Audio and exact 3D material fidelity remain unfinished.

## PS3 direct background composition (2026-10-12)

Revision 47 reaches at least 9120 frames and exits normally. Late windows
upload 1525-5731 of 46080 possible rows; layer upload still costs 3.8-4.1 ms,
CPU tile drawing 9.6-10.5 ms, and composition about 1.9-2.0 ms. Revision 48
enables the existing zero-margin direct background draw path for PS3_RSX_GAME
as well as Dreamcast: draw onto the pen-zero-cleared screen instead of clearing
sys24, drawing there and copying nonzero pixels back to screen. Foreground,
RSX shaders, row cache, controls and other platforms are unchanged. This reuses
the existing implementation but still needs PS3 visual/performance validation.
Audio remains absent; lighting/filtering remain approximate.

## PS3 exact layer row upload cache (2026-10-12)

User confirms revision 46 background/HUD are correct; log passes 3540 frames
and exits normally. Revision 47 keeps composition and shader output unchanged.
It compares source rows in CPU memory, uploads only changed rows, converts in
CPU scratch storage, and copies contiguous rows to RSX rather than alternating
per-pixel writes between two distant layers. Cache costs about 1.5 MiB CPU RAM;
initial rows always upload, including all-zero transparent rows. New RSX_LAYERS
timing reports upload cost and changed rows. CPU composition remains unchanged
and expensive; no performance gain claimed until hardware measurement. Audio
remains absent. Existing flip synchronization is preserved for all uploads.

## PS3 layer shader isolation after flip timeout (2026-10-12)

Revision 45 hardware log records flip timeout before its first 60-frame
checkpoint. This is a genuine presentation regression, not a manual exit.
Revision 46 separates the simple tile sampling fragment program from the 3D
material program; each pass explicitly binds its program and texture, including
the case where both programs use sampler zero. This isolates the combined
shader branch; it does NOT establish the root cause or prove a fix. Layer
vertex ranges fit the allocation and the linked draw routine accepts 24-bit
start indices. Build validation must be followed by hardware log/photo.

## PS3 background and HUD composition (2026-10-12)

Revision 44 log reaches at least 4920 frames and exits normally by user request.
Revision 45 bypasses the external System-24 GPU shortcut only for PS3_RSX_GAME,
allowing existing CPU reference composition to produce back/front tile layers.
RSX uploads them after flip completion into a 512x1024 atlas (2 MiB), draws the
background before geometry and transparent foreground after it. The layer
quads reserve 12 extra vertices outside the bounded geometry region and do not
write depth. This restores the missing presentation path, pending hardware
verification; CPU layer composition can reduce performance. Approximate 3D
lighting/filtering remain. Audio is still absent: the probe Runner neither
loads sound assets nor owns SoundBoard or a PS3 audio-output worker. Its native
sound sequencer must be connected separately, not represented as completed.

## PS3 gameplay input integration (2026-10-12)

Revision 44 feeds port-one controller input into the real board each frame.
Select coin, Start start, left stick/D-pad steering, Cross/R2 gas, Square/L2
brake, R1/L1 edge-triggered gears, Triangle/Circle views, L3 test, R3 service.
Select+Start exits before issuing coin/start. Pad state persists across empty
packets and releases on disconnect. Presentation no longer consumes the same
pad packets. Manual/XMB exits are normal, not a rendering failure. The current
36000-frame limit remains. Needs physical input/menu/race validation; missing
tile/HUD layers can still hide menus, audio remains absent, and lighting and
filtering remain approximate. This is not yet a complete playable port.

## PS3 extended viewing run (2026-10-12)

Revision 43 extends only --rsx-game from 600 to 36000 frames: ten minutes at
60 FPS, longer at lower throughput. Select+Start still exits early. Checked
software/reference runs stay at 600 frames; Xbox and other platforms unchanged.
Performance header now prints the actual limit. Rendering is unchanged from
revision 42; this is a duration change, not a performance or fidelity fix.
The initial build caught the 600-frame oracle-length assertion. That guard now
excludes PS3 performance mode, which already disables hash comparisons; checked
reference runs retain the assertion and their original validation requirements.

## PS3 decoded texture-index cache (2026-10-12)

Revision 41 completes 600 frames. No uploads occur during 3D frames; busy
scenes spend 17-20 ms in flip waiting, not texture upload (zero), vertex copy
(0.7-0.9 ms), or submission (~0.03 ms). This points to rendering/refresh cost,
not a pure GPU timestamp measurement. Revision 42 removes packed-byte/nibble
selection from fragments: generation-cached uploads expand both sheets into
a 2048x4096 RGBA index atlas (32 MiB, previously 4 MiB). The remaining shader
preserves address carries/wrapping and existing approximate shading. No uploads
on empty geometry frames, avoiding repeated expansion during initial ROM load.
The first nonempty frame can incur an upload stall. Timing instrumentation
remains; test performance and appearance on hardware before claiming a gain.
Host address tests compare old/new word and nibble selections across row/RAM
boundaries. Per-texel lighting, filtering, tile/HUD layers and audio remain open.

## PS3 texture-path cost isolation (2026-10-12)

Revision 40 completes 600 hardware frames. Photos confirm recognizable game
textures, with missing sky/HUD and approximate lighting. Tree-heavy frames
180-419 take about 32-33 ms/frame, versus about 16.7 ms late in the run.
Do not infer shader cost from display timing alone: it includes flip waiting.
Revision 41 leaves rendering unchanged and logs 60-frame RSX_TIMING windows:
flip wait, generation-cached texture upload, vertex copy/sync, command submit,
upload count/bytes, and vertex/batch counts. These are CPU-observed timings,
not GPU timestamp measurements; wait includes vblank and GPU completion.
Use this breakdown to select the next optimisation without guessing. Existing
texture filtering/lighting and layer/audio limitations remain unchanged.

## PS3 packed texture RAM sampling (2026-10-12)

Revision 39 completed 600 frames on hardware, with a photo showing perspective
checker patterns on road and cars. Revision 40 replaces synthetic checkers with
base-level nearest texture RAM sampling. A 4 MiB RGBA atlas holds both packed
guest-LE sheets; the fragment shader decodes the selected byte/nibble. Uploads
are generation-cached and occur after the previous flip completes. Vertex data
adds texture origin/size/mirror/sheet state (80-byte stride, maximum 10 MiB).
Texture index 15 discards on translucent textured materials. Lighting is still
an approximate index modulation, NOT the reference luma lookup. Bilinear/mip/
microtexture filtering, full wrap edge behavior, layers/HUD and audio remain
absent. Synthetic host tests check packed nibble selection; shader compilation
and package checks do not establish hardware appearance or game parity.

## PS3 perspective texture-coordinate diagnostic (2026-10-11)

Revision 38 hardware log completes all 600 frames, with approximately 60 Hz
partial-renderer throughput and zero CPU raster time. No new photo, so material
masking visual correctness remains unconfirmed. Revision 39 adds the original
game u/z, v/z and 1/z interpolants and fragment division, retaining synthetic
ordering depth and window scissors. Textured materials show a shaded checker
in actual game UV space; solid materials retain palette lighting. This is NOT
actual game texture decoding. Vertex stride is now 48 bytes, bounded at 131072
vertices (6 MiB). Nonfinite texture interpolants reject the polygon. Hardware
verification must check road checker perspective and seams; texture RAM,
filtering, per-texel lighting, tile/HUD layers and audio remain open.

## PS3 checker material masking test (2026-10-11)

Revision 37 completes 600 frames; user photo confirms flat palette-lit cars
and track. Textured surfaces remain approximate and are not visually faithful.
Revision 38 adds two reference material rules: renderer 1 solid-translucent
polygons draw nothing; checker flag discards even native-pixel parity in the
fragment shader before depth writes. Native pixel coordinates interpolate
from projected geometry, retaining 496x384 pattern scale at larger output.
Other textured transparency, actual texture decode/filtering/luma, backgrounds
and HUD remain unimplemented. Shader compile/package and five tests do not
prove hardware discard/depth behavior; request photo/log before proceeding.

## PS3 palette-lit geometry test (2026-10-11)

Revision 36 log completes 600 frames at roughly 60 Hz in sampled geometry-only
windows. No new photo provided, so clipping visual correctness remains open.
Revision 37 replaces synthetic checker mapping in --rsx-game with per-polygon
palette/color-translation/gamma lighting using the original solid equations.
Textured polygons are flat material approximations, NOT decoded game textures.
Vertex format is position4/color4 (32 bytes), isolated RSX test remains the
working textured quad. Window clipping/depth order unchanged. No HUD/layers or
audio. Next implement actual texture sampling and layer composition; do not
call this complete hardware rendering or infer finished-game 60 FPS.

## PS3 window-clipped hardware geometry (2026-10-11)

Revision 35 completed 600 frames with zero CPU raster time and approximately
60 Hz geometry-only throughput; photo confirms checker-covered game shapes.
This omits real textures/layers and is not complete-game 60 FPS evidence.
Revision 36 batches consecutive polygons by their original viewport rectangle,
uses the desktop/reference render-offset equations and transforms half-open
clip edges to RSX scissors in the 4:3 content area. Vertex ranges are bounded
before submission. Geometry/depth order is unchanged; no real texture/HUD yet.
Cross-build and existing unit tests are separate from pending hardware visual
validation. Next connect game texture decoding and background/HUD composition.

## PS3 real geometry RSX diagnostic (2026-10-11)

Revision 34 completes all 300 frames with alternating near/far submission;
user confirms near orange triangle over checkerboard. Revision 35 --rsx-game
connects Video external_3d and real sorted/projected polygon fans to RSX.
CPU 3D raster disabled; 600-frame bounded performance run with synthetic
checker mapping, no game texture decoding, no tile/HUD composition, no
per-window scissor yet. This is geometry integration, NOT gameplay fidelity.
Original 4:3 aspect retained. Vertex staging bounded at 131072 vertices, reused
only after previous flip completion. RSX depth uses draw-order rank, like the
desktop GPU path. No audio. Hardware geometry output and exit need testing.
Next implement per-window clipping and game textures/layers before claiming
equivalent output or using diagnostic timings as a finished-game FPS result.

## PS3 textured depth-order test (2026-10-11)

Revision 33 log completed 300 submissions/return path; user confirms orange/
cyan checkerboard visible. Textured shader draw now visually confirmed.
Revision 34 adds Z16 clear, LESS depth test/write and a near orange triangle
over the far checkerboard. Reverses submission order every 60 frames; output
must stay unchanged. Same bounded 300-frame test, no game rendering yet.
Visible stable occlusion and clean exit still require hardware validation.

## PS3 textured RSX geometry test (2026-10-11)

Revision 32 hardware log submitted 300 frames and reached XMB return; user
confirmed visible color bars. Revision 33 adds original passthrough vertex and
texture fragment shaders, a six-vertex quad and a 64x64 orange/cyan checker.
All drawing uses RSX, CPU only initializes texture/vertex resources. Still
isolated from ROM/game, bounded 300 frames. Requires visual/hardware validation.
Host cgcomp now works using public NVIDIA Cg 3.1 libCg.so kept in ignored
build/ps3-cg, not installed system-wide or packaged. Shader binaries generated
under build/. GetAttribIndex declarations lacked linked implementations; use
GetAttrib plus checked index instead. Cross-build/package and five tests pass.
Next: verify checkerboard quad, then game geometry/textures and composition.

## PS3 RSX render-target bring-up (2026-10-11)

User requests hardware rendering. Revision 32 adds isolated --rsx-probe:
GPU clears/scissors draw eight color bars and a moving gray rectangle into
double-buffered RSX surfaces. No CPU framebuffer writes in this mode, no ROMs,
no shader or game geometry. Bounded to 300 flips with callback/pad exit and
flip timeout; visual output/clean exit require hardware validation. This is
render-target infrastructure, NOT a completed hardware game renderer.
Desktop GPU path uses SDL_GPU storage buffers; it cannot be transplanted
unchanged. Installed cgcomp reports Unable to load Cg; shader dependency must
be resolved before shader-based textured geometry. No proprietary console SDK
used. Next: validate surface writes, then shaders/textured geometry, then game
integration against software reference. Do not claim 60 FPS from this test.

## PS3 lighting timing and 60 FPS boundary (2026-10-11)

hardware-lazy-shade-parity-v30.log confirms all 600 reference hashes PASS.
Revision 31 measures unchanged lazy polygon shades using --performance,
without hashes/traces. Compare with revision 29. Hardware speed pending.
User explicitly targets 60 FPS: total budget 16.67 ms; revision 29 sampled
raster alone is 58-88 ms. Do not present incremental CPU gains as sufficient.
Next architectural milestone: isolated RSX textured draw with completion/exit
validation, then native game polygon/texture support and composition, retaining
software output as reference. Installed PSL1GHT has rsx headers and cgcomp;
that is tooling evidence only, not proof of shader compatibility or gameplay.
No GPU rendering implementation or 60 FPS claim in this package. Audio/input
integration and full-race parity remain open.

## PS3 lazy polygon shade test (2026-10-11)

Revision 29 completes: raster 86555 / 87980 / 57996 us at frames 180-239 /
240-299 / 540-599 versus revision 27 86930 / 88541 / 58966 us; small changes.
Revision 30 adds a polygon-local 128-bit validity mask to reuse lazily computed
lighting results on short spans. Long spans retain the existing full table.
Same integer lighting equations, no shared or persistent cache, no FP changes.
Private Extra grows by 16 bytes; Raster layout unchanged. Reference parity
must pass before timing; performance benefit is not established.

## PS3 loop unrolling performance test (2026-10-11)

hardware-unroll-parity-v28.log confirms all 600 reference hashes PASS on PS3.
User reports it is still slow. Revision 27 performance is only about 8-11 FPS
in sampled 3D windows (123494 / 92429 us total frame at 180-239 / 540-599).
Revision 29 measures unchanged loop unrolling using --performance, removing
hash/trace overhead and retaining comparable span timers. Compare to revision
27; do not attribute parity-mode wall time to runtime performance. Small CPU
changes have not closed the gap to 60 FPS; larger rendering architecture work
needs investigation, not a claim that this test enables GPU rasterization.

## PS3 raster loop unrolling test (2026-10-11)

Revision 27 completes: raster 86930 / 88541 / 58966 us at frames 180-239 /
240-299 / 540-599 versus revision 25 91368 / 92984 / 61158 us (about 4-5% less).
Keep integer coordinate conversion. Revision 28 tests -funroll-loops for
raster.cpp only; strict FP, no contraction and all other flags stay unchanged.
Unrolling is experimental: code-size/instruction-cache cost can outweigh loop
overhead savings. First run 600-frame reference parity; speed not yet measured.

## PS3 integer coordinate performance test (2026-10-11)

hardware-coordinate-parity-v26.log confirms all 600 reference hashes PASS on
PS3. Revision 27 switches unchanged coordinate conversion to --performance,
without reference hashes/traces. Compare with revision 25 to measure the
conversion change. All other renderer and compiler settings remain unchanged.
Speed benefit awaits hardware; startup parity is not full-race validation.

## PS3 integer coordinate conversion test (2026-10-11)

Revision 25 completes: raster 91368 / 92984 / 61158 us at frames 180-239 /
240-299 / 540-599 versus revision 23 92794 / 94804 / 62310 us. Retain byte reads.
Revision 26 tests IEEE binary32 bit-based truncation for PS3 to_s32 only,
preserving INT32_MIN for NaN/out-of-range and signed zero/subnormal handling.
No changes to perspective FP arithmetic. Host ASan/UBSan passes 4,003,072
comparisons (all exponent/sign combinations at mantissa edges plus random
patterns); five Python tests pass. Initial test compile needed initializer_list
include, now corrected. 600-frame hardware parity and speed still pending.

## PS3 byte texel performance test (2026-10-11)

hardware-byte-texel-parity-v24.log confirms all 600 reference hashes PASS on
physical PS3. Revision 25 switches unchanged byte texel reads to --performance
without accuracy hashes/traces. Compare against revision 23 for the texture
read change; compiler settings, lighting tables, mip caching and covered-span
rejection are unchanged. Speed benefit pending hardware; startup parity is
not a full-race validation.

## PS3 byte texel parity test (2026-10-11)

Revision 23 completes. Tile drawing at frames 240-299 drops from 10403 to
9438 us; composition rises from 5413 to 5574 us. Retain strict O3 video.
Revision 24 changes only PS3 scalar texel fetch to read the selected byte
directly from little-endian texture storage, avoiding a full word endian swap.
Original folded address/mask and local coordinate nibble selection stay exact.
Host ASan/UBSan passes 8,388,608 comparisons covering all packed addresses,
wraparound and parity combinations. Five Python tests pass. Console 600-frame
parity and speed remain pending, following Platform layer & build validation.

## PS3 strict O3 video performance test (2026-10-11)

hardware-video-o3-parity-v22.log confirms all 600 reference hashes PASS on PS3.
Revision 23 switches unchanged rendering to --performance without reference
hashes/traces. Compare tile_draw and composite against revision 21 to measure
video.cpp's compiler change. Raster remains strict -O3; other units -O2.
Five existing unit tests and package validation are checked separately from
pending hardware timings. Startup parity does not establish full-race fidelity.

## PS3 strict O3 video test (2026-10-11)

Revision 21 completes. Raster 92270 / 94124 / 63264 us at frames 180-239 /
240-299 / 540-599, versus revision 19 93635 / 95671 / 63269 us: modest gain
in early sections, essentially unchanged at the end. Tile draw/composition
still total about 16 ms/frame. Revision 22 extends strict -O3 to video.cpp
only; raster settings unchanged, other units remain -O2. Video is now always
rebuilt under --reuse-runtime-objects to avoid stale compiler settings.
No fast-math or FP contraction; 600-frame accuracy test before performance.

## PS3 strict O3 performance test (2026-10-11)

hardware-o3-parity-v20.log confirms all 600 reference hashes PASS on PS3.
Revision 21 switches the unchanged strict -O3 rasterizer to --performance,
removing reference hashes/traces but retaining scanline timers. Compare with
revision 19 to measure the compiler change. Other compilation units remain
-O2, with no fast-math or FP contraction. Speed benefit awaits console results;
the startup reference pass does not establish full-race correctness.

## PS3 strict O3 raster test (2026-10-11)

Revision 19 completes: raster 93635 / 95671 / 63269 us at frames 180-239 /
240-299 / 540-599, versus revision 17 98824 / 100808 / 68000 us (5-7% less).
Retain polygon-local lighting caching. Revision 20 tests -O3 for raster.cpp
only, preserving -fno-fast-math and -ffp-contract=off and -O2 for other units.
No other platform flags change. The raster object is always rebuilt even with
--reuse-runtime-objects. Package uses 600-frame reference validation first;
neither hardware parity nor performance is established for this compiler change.

## PS3 polygon lighting performance test (2026-10-11)

hardware-lighting-parity-v18.log confirms all 600 reference hashes PASS on
physical PS3. Revision 19 retains polygon-local lighting tables, cached mip
addresses and covered-span rejection. Uses --performance without per-frame
accuracy hashes/traces, retaining scanline timers for comparison to revision
17. No further rendering changes. Lighting speed benefit awaits hardware
measurement; the 600-frame pass does not establish full-race correctness.

## PS3 polygon lighting parity test (2026-10-11)

Revision 17 completes: raster 98824 / 100808 / 68000 us at frames 180-239 /
240-299 / 540-599, versus revision 15 102111 / 104881 / 71452 us (3-5% less).
Revision 18 retains mip caching and tests polygon-local 128-color lighting
tables, built lazily when a span reaches 32 pixels. Short polygons retain the
original path. Uses identical integer lighting equations and no persistent
palette cache; adds 512 bytes to private Extra, not Raster object layout.
600-frame reference validation is required before any performance claim.

## PS3 mip address performance test (2026-10-11)

hardware-mip-parity-v16.log confirms all 600 reference hashes PASS on PS3.
Revision 17 keeps cached mip addresses and covered-span rejection, switching
to --performance to remove accuracy hashes/traces. Compare against revision 15
to isolate mip-address caching. No new rendering changes; speed benefit is
pending hardware measurement, following Platform layer & build validation.

## PS3 mip address parity test (2026-10-11)

Revision 15 completes; covered-span rejection shows only small differences
from revision 11 (102111 vs 103224 us at frames 180-239, 71452 vs 71363 us
at 540-599), not an established material speedup. Retained for this next test.
Revision 16 enables only existing per-polygon mip address/size precomputation
on PS3, leaving scalar texel reads, lighting and FP interpolation unchanged.
Extra is private to raster.cpp; Raster object layout is unchanged. Mip levels
0..11 plus microtexture fit 13 entries. Run 600-frame parity before timing.

## PS3 covered span performance test (2026-10-11)

hardware-covered-span-v14.log confirms all 600 frame hashes PASS on PS3.
Revision 15 retains that covered-span rejection and scalar texture reads,
switching to --performance without per-frame hashes or diagnostic traces.
Retains scanline timing for comparison with revision 11 baseline. No further
render changes; hardware speed benefit remains unmeasured. This follows the
design document Platform layer & build accuracy-before-performance sequence.

## PS3 covered span parity test (2026-10-11)

Revision 13 completes but packed texture reads regress: raster time at frames
180-239 / 240-299 / 540-599 is 108724 / 110731 / 75760 us, versus revision 11
103224 / 105647 / 71363 us. Reverted PS3 packed reads; Vita remains unchanged.
Revision 14 instead enables the existing fully-covered span rejection on PS3.
Four-byte memcpy checks skip spans only when every fill byte is 0xff; partial
spans preserve all original FP interpolation additions. No class layout change.
Following Platform layer & build parity-first rules, package uses 600-frame
reference comparison, not performance mode. Accuracy and speed pending hardware.

## PS3 packed texture performance test (2026-10-11)

hardware-texture-parity-v12.log confirms all 600 reference frame hashes PASS
on physical PS3 with packed texture reads. Revision 13 keeps that exact path
and switches back to --performance, removing per-frame hashes and diagnostic
traces. Retains revision 11 scanline timers for like-for-like measurements.
Compare textured span and raster timings against revision 11; hardware speed
improvement remains unmeasured. No lighting, quality or gameplay changes.

## PS3 packed texture read parity test (2026-10-11)

Revision 11 completed all 600 frames. Textured spans account for 82-96 percent
of raster time; solid fills are below 0.2 ms/frame. Revision 12 enables only
the existing read_texel_quad helper on PS3: aligned adjacent footprints share
one packed-cell load, other footprints retain independent reads. No shade
cache, mip-level precomputation, FP approximation or Raster layout changes.
The design document's Platform layer & build parity-first boundary applies:
this package uses --game-display (600 reference hashes), not --performance.
PS3 plane normalization correction and cached display scaler stay enabled.
Host ASan/UBSan test passes 2,000,000 footprints / 8,000,000 scalar comparisons;
five Python tests pass. Big-endian console parity and speed remain untested.
Next: require 600 hardware hashes to pass before measuring performance again.

## PS3 scanline profiling test (2026-10-11)

Revision 10 hardware log completes 600 frames. 3D rasterization costs 71-109 ms
per frame, tile drawing about 10 ms, presentation about 11 ms; geometry 3-6 ms.
Revision 11 adds PS3_PERF_PROBE-only scanline timers and counters, with no
changes to rendered arithmetic. SPANS kinds: 0 solid opaque, 1 solid translucent
(no drawing), 2 textured opaque, 3 textured translucent. Times are per-frame
means; calls and candidate_pixels are totals for the preceding 60-frame window.
Candidate pixels include occluded/checker pixels, not actual texture fetches.
Timer overhead is included; compare with revision 10 before estimating setup
cost by subtracting span times from raster time. No per-pixel timer overhead.
Build/package validation and five unit tests pass; hardware results pending.

## PS3 renderer timing breakdown (2026-10-11)

hardware-scale-v9.log confirms revision 9 completes. In 3D, display now averages
11.2-11.4 ms (previous ~48 ms); scale/copy ~10.7 ms, wait/input ~0.1 ms.
Game/render remains 95-135 ms. Revision 10 enables existing VideoProfile clock
and adds per-60-frame BREAKDOWN summaries: geometry/vblank-start, total video,
tile cache, tile draw, CPU raster, composition, and remaining execution time.
Geometry bucket includes vblank-start IRQ/communication overhead; remaining
execution includes CPU/TGP dispatch and callbacks, not pure game logic.
Video sub-buckets are nested in video_total, not additive to it. No gameplay
arithmetic changes or frame skipping; cached scaler and plane correction stay.

Revision 10 cross-build, ELF/SELF/PKG validation and five unit tests passed.
Uploaded to /dev_hdd0/packages/daytona-ps3-probe.pkg and verified by download:
SHA256 37deb91867feb56fdd8a90ee49c63ad64991c738316ee53559c5f4d5691f03ea.
Requires package reinstallation and hardware run; new timing results pending.

## PS3 cached presentation scaling (2026-10-11)

Revision 8 hardware-performance.log completes 600 frames. In 3D, game/render
averages 92-133 ms and presentation about 48 ms: roughly 5.5-7 FPS total.
Revision 9 changes only display scaling: cache horizontal sample coordinates,
build a row in main RAM, reuse it for duplicate source rows, memcpy to scanout.
No VRAM readback, same 4:3 nearest-neighbour sampling and XRGB masking.
ASan/UBSan test_scale.cpp passes 25,762,088 exact pixel comparisons across modes,
source widths, repeated frames and dimension changes, with output guard words.
Adds per-60-frame wait/input, scale/copy and submit timing breakdown. Real
hardware speedup unmeasured until next log. Game rendering itself unchanged.

Revision 9 cross-build/SELF/PKG and five existing tests passed. Uploaded PKG
SHA256 c3e6b6e57b5b20d3327d95450deeb6fc68d092623cc384a8e438490f88eb2f9a.
Compare next hardware PERFORMANCE/PRESENT timings with revision 8 baseline.

## PS3 performance baseline (2026-10-11)

hardware-plane-fix.log verifies all 600 frames match the unchanged host oracle
after the PS3 plane_hypot correction. User observed progress but slow execution.
Revision 8 adds --performance: identical 600-frame startup without per-frame
RAM/register/pixel hashing or geometry/raster traces. Keeps ROM validation and
the corrected math. Logs per-60-frame game/render and presentation mean/max
microseconds plus elapsed milliseconds, using PPU timebase and SDK frequency.
Presentation includes flip waiting. Completion is explicitly NOT a parity pass.
No CPU scaling/render optimization yet: first obtain a measured baseline.
No audio/gameplay input; bounded hold and automatic return remain.

Revision 8 cross-build and package checks pass; five existing tests pass.
Executable inspection confirms raster/clip diagnostic symbols and per-frame
hash messages absent. Uploaded package SHA256:
279ed5696b119913ae02ac85d7ff9f1139146702607841cb63ae847a67fc07f0.
Hardware timing results pending; do not claim FPS improvement yet.

## PS3 plane normalization correction (2026-10-11)

Revision 6 hardware-clip.log first differs at record 0: plane normal pz is
3f7fff78 host vs 3f7fff79 PS3, all incoming vertices equal. Linked .hypotf
disassembly shows scaled float divides, fmadds, fsqrts and float multiply.
Thus the prebuilt library contains fused arithmetic even though geo/raster do
not. Source inspected earlier generic newlib ef_hypot.c is NOT the linked
implementation; executable disassembly is the evidence for this target.

Revision 7 changes only the PS3 float-hypot compatibility shim to plane_hypot,
double square/sum/sqrt then float rounding (infinite arguments handled).
Host sweep passes all 524284 normalized components for integer planes from
-65535 through +65535 with both signs. No whole-game correctness claim until
hardware run. Rebuild geo/raster/frontend; preserve fresh host reference and
unchanged generated execution objects. Diagnostic traces remain enabled.

Revision 7 target build/SELF/PKG passed, five existing unit tests pass. Object
inspection confirms double fsqrt and no hypotf call in corrected geo.cpp.
Uploaded PKG SHA256 6cb8bfc661da2ea9c7bddccbb63a90a43b114e35feec86ef947786df22fe8df7.
Next fetch console log; require frame 181 and remaining startup hashes to pass.

## PS3 clipper boundary trace (2026-10-11)

Revision 5 hardware-geometry.log shows differences already in stage-0 geometry
vertices, before projection. Raster interpolation is downstream, not established
as the origin. Compiled geo.cpp also contains no fused multiply-add instructions.
Revision 6 traces frame-181 clipping input vertices and plane normals, plus
crossing-edge dot products, scale and resulting vertices. Math is unchanged;
do not substitute a guessed hypot/rounding fix before comparing this trace.
compare_clip.py reports the first differing record/word. --reuse-runtime-objects
now always rebuilds geo.cpp and raster.cpp for diagnostic changes.

Revision 6 build/SELF/PKG passed. Host 600-frame run unchanged, 7188 clipping
records captured; self-comparison and five existing tests pass. Uploaded PKG
SHA256 04764aa19cf234a5ce65df66dd8a771c5422f3c5030ebe1b43f4cb0b80eb78f2.
Await hardware log; no arithmetic fix claimed. Failed-test automatic exit stays.

## PS3 geometry/interpolation boundary (2026-10-11)

Revision 4 hardware-raster.log confirms selected UV fixed-point coordinates
differ by one, changing filtered texel and final color. Differences already
exist in scanline start/step floats; lighting inputs match. This does not yet
identify whether source vertices, projection, or edge interpolation first differ.
No fused multiply-add instructions found in compiled raster object; do not
assume FMA is the cause simply because the target is PowerPC.

Revision 5 records pre-projection vertices (stage 0), projected/perspective
vertices (stage 1) and polygon-edge intermediates (stage 2), only emitting
records for the three selected pixels in frame 181. Rendering math unchanged.
Host and PS3 traces use the same field layout; vertex records contain count
then x,y,p0,p1,p2 per vertex. Edge fields are documented inline in raster.cpp.

Revision 5 build/link/package passed. Host completes 600 frames with unchanged
frame-599 hash and captures nine geometry records (three stages per pixel).
Parser self-comparison and five unit tests pass. Uploaded package SHA256:
b369e5b03aef191ef5a1c9eeff05a1ee44d2e9db67448d97c862261fe0b3fc84.
Await hardware trace; no precision/rendering fix claimed.

## PS3 targeted raster evidence (2026-10-11)

Revision 3 hardware-components.log and hardware-frame181-state.bin confirm
CPU/main RAM/work RAM/TGP count all match. Exactly 22 of 190464 output pixels
differ at frame 181, by small color changes. Exit reached "returning to XMB".
No evidence yet whether interpolants, texture samples or lighting cause this.

Revision 4 preserves arithmetic and captures float bit patterns, fixed-point
UV, mip selection, filtered texel and shading inputs for pixels (9,41),
(204,239), (409,316) during frame 181. Same instrumentation on host and PS3;
M2_RASTER_DIAGNOSTICS is off for other builds. Rebuilds raster.cpp even with
--reuse-runtime-objects. compare_raster.py compares private log records.
Do not waive image parity or claim a precision fix without this evidence.

Revision 4 builds and packages successfully. Host still passes 600 frames;
three RasterPixel records captured, parser self-comparison passes, five unit
tests pass. Uploaded PKG SHA256
86ec2a2e91ff0829e096c4e10f436a7d4b5b14b1f851c50188d19551efa3c78a.
Await physical PS3 log, then compare_raster.py against current host-game.log.

## PS3 frame-181 divergence (2026-10-11)

User photo confirms game HUD pixels displayed, but Start+Select did not exit
and user restarted. hardware-display-game.log shows frames 0-180 matching;
181 diverges (host 42849d0a7d05f77d, PS3 007b9e1ca2dd540f), then normal FAIL
and final-frame hold. This is not evidence of a CPU crash. Instruction/TGP
counts and IP match at 181; differing registers, RAM or pixels not yet isolated.

Revision 3 adds independent CPU/main RAM/work RAM/video/TGP-count hashes at
frames 178-181 and a private little-endian frame181-state.bin beside roms.
compare_state.py reports differing word indices without printing memory data.
Failure now returns to XMB; success hold is bounded to about 15 seconds.
This avoids relying on the unresolved controller exit, not a claim it is fixed.
No game arithmetic/rendering changes until component evidence is available.

Revision 3 cross-build/SELF/PKG passed, host still completes 600 frames, five
header/state-parser tests pass. Host frame181-state.bin parses successfully.
Uploaded package SHA256 77b4ce2025688d028dd8d1cf3e1f318426e8004c1ca1ed39ed1e86ce4aae08be.
Next retrieve both ps3-boot.log and frame181-state.bin from title USRDIR;
save the console dump under a different name to preserve the host reference.

## PS3 game presentation test (2026-10-11)

Hardware-game.log confirms all 120 startup frame hashes matched the host,
Runner destroyed, final PASS; user confirms clean XMB return. Next test adds
600-frame oracle and RSX presentation, retaining the checked CPU framebuffer
and 4:3 aspect. Final image stays on screen until Start+Select or XMB exit.
No audio or gameplay controls yet; actual visible output remains untested.
Presentation does not change simulation inputs, timing boundaries or renderer.
Original runtime/generated objects may be reused for this frontend-only change;
never use --reuse-runtime-objects after runtime/header/toolchain changes.

600-frame host run passed; frame 599 hash 2c5c1250f587b1d5. Full target
link, SELF/PKG validation and three header tests passed. New package SHA256
f3d79c5e23c7022275744863b59dff8108321155a2ee0531ed16615bb18f2f45,
uploaded to .64 /dev_hdd0/packages/daytona-ps3-probe.pkg. Existing ROM data
unchanged. Reinstall before testing. Hardware display/parity results pending.

## PS3 real-runtime integration (2026-10-11)

Retrieved hardware-cxx.log: C++20 vector/span/bit_cast/exception/main-thread
TLS PASS, RSX 1920x1080, 120 flips, Cross press/release, clean exit. PS3 FTP is
now 192.168.1.64, not .83. Worker-thread TLS is not validated.

Added --game-probe to PS3 builder: isolated ps3-host-tools generation, fresh
120-frame host oracle, shared bounded Runner with native PS3 paths/callbacks.
Xbox TLS/exception wrappers are excluded. It returns to XMB after comparison;
no presentation or audio in this test. Six verified ROM images are separate
from PKG, in installed title USRDIR/roms (about 75 MiB).

First compile exposed missing C99 declarations under strict C++20; adding
cstdio alone did not solve it. GNU++20 exposes C declarations, but this SDK's
libstdc++ configuration omits several std namespace exports. Target-only
cxx_compat.h imports existing functions and preserves float hypot via hypotf.
Runtime syntax check passes with this shim; full build/hardware results below.

Full GCC 13.2 compile/link and SELF/PKG generation passed. Fresh host oracle
completed all 120 frames and matches existing Xbox host-reference hashes;
three package-header unit tests and git diff --check passed. Target ELF has
5,104,483 bytes text, 13,372 data, 265,440 BSS (not the dynamic memory budget).
Generated chunks compile slowly; builder now uses four workers. The final
target-only rebuild reused the freshly generated oracle, with unchanged runtime.
PKG SHA256: 37c3c9518e3c42c6d89cb4746e6af341a8fcd9a5852cb284b9cf86d5852a8395.
Uploaded to /dev_hdd0/packages/daytona-ps3-probe.pkg; six USRDIR/roms assets
uploaded and verified by downloaded SHA256. User must reinstall and launch;
blank screen then XMB is expected. Fetch ps3-boot.log and require 120 hashes
plus final PASS before presentation integration. Real PS3 runtime is untested.

## PS3 isolated C++20 toolchain (2026-10-11)

User photo confirms colour bars; retrieved display log reports RSX initialized,
1920x1080, 120 flips submitted and leaving probe. No Cross events were logged.
Evidence saved to build/ps3-current/hardware-display.log.

Copied working SDK to build/ps3-sdk and built upstream ps3toolchain GCC 13.2
plus its target runtime there. System /usr/local/ps3dev remains GCC 7.2. Sources
and logs are in build/ps3-toolchain-source; preserve both build dependencies.
Pinned script revision e13aa539c07abe86cdbcd095dc1340492dbca213. Host Clang
MPFR float128 detection conflicted with glibc: --disable-float128 resolved it.
Old newlib makedoc then needed -Wno-error=incompatible-pointer-types and
-Wno-error=int-conversion for CFLAGS_FOR_BUILD only. Target checks unchanged.
upgrade_toolchain.sh records the reproducible staging/configuration approach.

New compiler passes --check-cxx (C++20 span/bit_cast/vector, exceptions and TLS
linking). --runtime-probe packages those checks before the existing C display.
Host ASan/UBSan check passes with leak detection disabled for sandbox ptrace
restrictions. Three ELF-header tests and package/header checks pass. Hardware
C++ runtime validation remains pending; no game, worker TLS or audio claims.
Uploaded package only to /dev_hdd0/packages/daytona-ps3-probe.pkg; readback
SHA-256 matches 918e001a55603bb42de3acf4113d08c6df86ce17c13274242d65b27c82c33eb6.
Reinstallation is required; nothing remotely installed/launched.

## PS3 initial display/input port (2026-10-11)

Branch ps3 starts at Xbox bring-up commit 8ecaa13. Added platform/ps3 with a
ROM-free RSX CPU-written double-buffer display test, pad input, XMB callback,
bounded flip polling and persistent log. Build script produces SELF and PKG
in build/ps3-current (DAYTPS301); no SDK modifications, deployment or commit.
Uses documented PSL1GHT RSX APIs, no copied sample implementation or shaders.
Design basis: platform-layer bring-up before game integration and parity before
optimization. Existing PC/mobile/Vita/Xbox build paths are unchanged.

Installed /usr/local/ps3dev compiler is GCC 7.2. Strict C11 failed on SDK asm
extensions; GNU C11 builds. The explicit C++20 span/bit_cast/exception/TLS
probe fails because -std=c++20 is unsupported. Do not claim the game compiles
or solve this by dropping its language requirements. Next toolchain work needs
an isolated newer compiler/library with verified PS3 ABI, not an SDK overwrite.

Display probe ELF64 big-endian PPC64, SELF generation and package listing pass.
PKG SHA-256: 4131075f074f14dd7add4f7e7c769dde237ed64a2c102dbbe67fe603409ed899.
Contains PARAM.SFO and EBOOT.BIN only, no game data. Hardware is untested.
Expected screen: colour bars/moving strip; Cross turns lower field green;
Start+Select or PS/XMB exits. Log: installed USRDIR/ps3-boot.log, USB000 fallback.
Audio/game runtime remain unimplemented; no gameplay/performance claims.

## Xbox presentation investigation boundary (2026-10-11)

Physical-buffer hardware log passes: virtual ffb2c000, physical 1fb2d000,
1,228,800 bytes, every page mapping contiguous, zero byte mismatches, free
completed and parking reached. Evidence: build/xbox360-current/hardware-framebuffer.log.
Do not derive physical addresses from virtual pointer masking: this observed
mapping differs by a page from that assumption; use MmGetPhysicalAddress.

Next GPU presentation step is not implemented or uploaded. Rechecked the
pinned SDK and open-source alternatives. Xenia VdInitializeEngines remains
a stub with unresolved parameters and external microcode pointers; VdSwap
writes an emulator-specific PM4_XE_SWAP packet. Free60 libxenon's xenos driver
directly writes GPU/ANA/SMC registers and assumes its own hardware environment.
It is not a drop-in kernel-cooperating XEX backend. No such code was copied.
The earlier shutdown/red-light cause remains unresolved. Do not substitute
another no-display allocation test for completion of the presentation step.

Required before a console GPU submission:
- Establish kernel-cooperating engine ownership, initialization and callback
  ABI, with legally sourced prerequisites, without speculative arguments.
- Validate ring allocation, cache/order rules, submission and completion with
  timeouts; never free buffers until the GPU has demonstrably released them.
- Establish scanout resource format, scaler/present parameters and Guide-safe
  teardown before connecting CPU game pixels. First visible test is color bars.
- Only after visible confirmation, connect the unchanged CPU reference image;
  retain game hashes and keep audio/optimization separate.

Existing XEX on the Xbox remains the passing physical-buffer probe. No new
build, upload or launch was performed during this investigation.

## Xbox physical framebuffer prerequisite (2026-10-11)

Display-query hardware log passed output guards and reached parking. Reports
640x480 front buffer, 1920x1080 output, refresh bits 0x42700000 (60 Hz).
Raw evidence: build/xbox360-current/hardware-display.log. No presentation
has been tested. Xenia's VdInitializeEngines is a stub with unresolved arguments;
its VdSwap emits emulator-specific commands, not a native implementation.
Do not transplant that code or guess initialization arguments on the console.

Next isolated --framebuffer-probe allocates 1,228,800 bytes (640x480x4) with
4096-byte alignment and write-combine protection. It checks every page's
physical mapping, writes/reads an explicit byte pattern with a PPC sync barrier,
frees the allocation and parks. No GPU submission, register access, display
changes, ROMs or CRT exit. Log: xbox360-framebuffer.log. This is memory-path
validation, not a display test or confirmation of a GPU pixel format.
Native GPU initialization/scanout remains the presentation blocker.
Built and mapped-image checked; 24 tooling tests and diff checks passed.
Uploaded only default.xex, remote checksum verified:
85f035b0e4a11c45367af047008bbf8653bc33a29c52f6a505daa4be401776de.
Hardware run pending; no remote launch or ROM/log replacement.

## Xbox display query prerequisite (2026-10-11)

Hardware 120-frame reduced-stack log passes every reference hash, cleanup,
30 heartbeats and parking. User returned to dashboard. Evidence remains in
build/xbox360-current/hardware-120-frame.log. This is startup parity only.

Next step is ROM-free display ABI discovery, not visible game presentation:
build.py --sdk build-xbox360-sdk --display-probe --out build/xbox360-current.
Installed SDK VdGetCurrentDisplayInformation lacks its pointer argument;
an independent declaration and 0x58-byte output layout are based on the pinned
BSD Xenia reference. Guarded static output storage avoids large stack frames.
The probe logs before/after query, guards, dimensions/formats and raw output
to xbox360-display.log, then parks without automatic CRT exit. It makes no
GPU initialization, MMIO, clock, scaler or display-mode writes. Presentation
is blocked on a verified native initialization/scanout path; do not call the
SDK's incomplete GPU declarations speculatively. Earlier red-light cause is
still unknown. This query requires hardware validation.
Build and mapped-XEX checks passed, plus 23 tooling tests and diff checks.
Uploaded only default.xex and verified remote readback SHA-256:
ac7067471a1eab673d49ed858f7872e355fd38f775a8896a1b8544a1be227081.
No launch performed; ROMs and prior logs retained.

## Xbox longer reduced-stack comparison (2026-10-11)

Retrieved the reduced-stack hardware log: all four frame signatures pass,
including frame 3 CPU composition, runner destruction, TLS cleanup and all
30 observation heartbeats. The final marker is parking without CRT exit.
User returned through Guide to the dashboard. Saved evidence at
build/xbox360-current/hardware-four-frame-pass.log. This supports the smaller
draw-stack path for this bounded run, not a proven stack-overflow cause or
an explanation of the earlier shutdown/red-light report.

Next comparison: build_game_trace.py --paced-startup --small-draw-stack.
It rebuilds video with the same heap mask but no detailed video checkpoints,
runs 120 reference frames with existing pacing, and parks without automatic
exit. Other platforms are unchanged. Hardware execution remains pending;
no graphics presentation/audio or automatic remote launch is added.
Validation: all 120 host ASan/UBSan frame hashes match the unchanged baseline;
mapped-image/unwind checks pass (76 CIEs, 1126 live FDEs, 52 discarded), as do
22 Python tests and git diff --check. New XEX SHA-256:
a3695ea8c55e6921abf04ab1eba6c81bd2cff0c4673d1672e6b80781141e8a3b.
Uploaded only default.xex to Hdd1/Games/Homebrew/Daytona and verified the
downloaded copy matches this hash. ROMs/logs untouched; no remote launch.

## Xbox tile draw stack comparison (2026-10-11)

Latest retrieved hardware log: screen 0x483e0010, 190464 words; sys24
0x484a0010, 192448 words. Both counts match and both fills finish. Last
marker is Video after sys24 clear; before first draw. Draw entry is absent.
Object disassembly shows draw's prologue reserves 4304 bytes before its first
checkpoint, including a uint16_t mask[0x800] (4096 bytes). This supports a
stack/call-entry investigation, not a confirmed stack-overflow diagnosis.
--trace-video now also enables M2_XBOX360_SMALL_DRAW_STACK, moving that mask
to a per-call RAII heap allocation without changing Video layout or other
platform paths. Four-frame reference comparison remains required. This is a
bounded diagnostic allocation strategy, not a claimed production optimization.
Disassembly confirms stack frame reduced from 4304 to 208 bytes. Four host
ASan/UBSan hashes, mapped XEX/unwind checks (76 CIEs/1129 FDEs) and 22 Python
tests pass. Uploaded only default.xex to the same Xbox folder; readback SHA-256
matches a545542e94120c5e9c5d7521c8651a7fb4eb6411e1b7cea956b84c2fd02b568d.
No launch performed; console outcome pending. Mode line identifies tile draw
mask on heap; small-stack comparison.


## Xbox video buffer boundary trace (2026-10-11)

Retrieved latest hardware log over the user's FTP after reported shutdown.
Frame 3 passes palette and all four build_layer calls, then ends at Video
before layer composition. No Video draw entry appears. Added v2 checkpoints
before the background clock, before/after screen fill, before/after sys24
clear, plus address/count/expected-size logging for both vectors. Incorrect
buffer size/null data raises a caught diagnostic error before filling.
No root cause or crash fix claimed: draw prologue remains a possibility, as
do the fills or corrupt state. Same four-frame host reference required.
V2 built: all four ASan/UBSan host hashes match, screen/sys24 counts are
190464/192448 as expected; mapped XEX/unwind checks pass (76 CIEs, 1129 FDEs),
22 Python tests pass. Replaced only default.xex in the Xbox Daytona folder;
FTP readback SHA-256 matches local 476a487e1e4449f4ddd8619c0749f381fe5941921c7f9f95a88e2133109b28b6.
ROM files unchanged; no launch performed. Next hardware log should identify
buffer checks v2 in its mode header. Not a verified crash fix.


## Xbox video-trace build restored (2026-10-11)

Rebuilt missing isolated compiler/linker, exception runtime, host tools and
real-game baseline from the existing pinned toolchain sources and owned ROMs.
Baseline final 120-frame hash remains 23c8b9fbbbcea1b2. PPC REL24 check passes
forward/backward >8 MiB and rejects overflow/misalignment. --trace-video now
packages build/xbox360-current/default.xex successfully: four ASan/UBSan host
hashes match, video phase checkpoints appear, mapped XEX/unwind checks pass
(76 CIEs, 1128 live FDEs), and 22 Python tests pass. Console test pending.
Runtime/generated baseline objects were rebuilt, not recovered bit-for-bit;
baseline.json records the fresh objects plus instrumented runner/video hashes.
Previous hardware trace is retained as xbox360-current/hardware-last.log.
Deployed the new XEX and all six ROM images to the user's Xbox at
Hdd1/Games/Homebrew/Daytona/. KDE directory access worked; its .part upload
was rejected. Direct FTP in the authorized desktop context transferred files.
Remote ROM sizes match local assets; downloaded XEX SHA-256 matches local:
49b8bdea529ed337f539d5968003ed0e544855b46bff1ec009018d0e1e7a1c3f.
No automatic launch performed. Next evidence is the console's new
xbox360-game-trace.log, starting with CPU video phase checkpoints enabled.


## Xbox frame-three video boundary (2026-10-11)

Latest hardware trace stops at Before vblank_end, count 0x94000, IP 0x53ec.
There is no After vblank_end. M2Board::vblank_end assembles VideoMem and calls
Video::screen_update; crash localization now targets CPU composition, not exit.
Added --trace-video: frame-three dispatch trace plus optional video checkpoints
around palette/decode/build-layer/draw/raster/composite. Only runner and video
objects are rebuilt against the baseline; other platform builds have no active
trace hook. No rendering fix or new hardware pass claimed.

Build was blocked: build/ contained only xbox360-current; retained compiler,
runtime, baseline and generated sources from the prior cleanup were now absent.
SDK, toolchain source and source ROM archives remain. Started rebuilding the
isolated compiler and host game generation. Latest default.xex is STILL the
previous fourth-frame probe until --trace-video successfully packages it.
22 Python tests and video syntax checks (trace on/off) pass; new XEX pending.

## Xbox build output consolidation (2026-10-11)

User requested removal of accumulated builds and reuse of a single output
folder. build_game_trace.py now writes every mode to build/xbox360-current.
Latest fourth-frame artifacts moved there; missing default.xex regenerated
from its existing validated package.exe. Six obsolete game diagnostic folders
removed (trace, startup, second-frame, quiet-slice, paced-slice, paced-startup).
Required xbox360-game baseline including ROMs, host tools/generated sources,
compiler, SDK and runtime libraries are retained. Older paths below are history.

## Xbox frame-three crash isolation (2026-10-11)

Latest paced-startup log passes frames 0, 1 and 2 (frame 2 hash
f18f5feeb7a625da, count 0x6f001, IP 0x16f8), then stops at Begin frame 3.
User explicitly reports a crash, not shutdown on this run; a shutdown on retry
is a prediction, not observed evidence. Cleanup/parking/CRT return were never
reached. Thus the previous exit-path suspicion cannot explain all failures.

Added --trace-fourth-frame: same unsliced frame 0, paced quiet frames 1 and 2,
then detailed frame-3 checkpoints around dispatch, vblank and state hashing.
Stops after four frames, retains post-cleanup hold and no automatic exit.
Output build/xbox360-game-fourth-frame/default.xex; same ROMs and device log.
Host validation rejects tracing in earlier frames and checks all four original
hashes. Added logging changes latency/layout and may mask the fault; no fix
claim. Native execution/Guide exit still require hardware testing.
Built successfully: ASan/UBSan host run matches all four original hashes;
trace confinement to frame 3 verified. XEX mapped extent/unwind checks pass
(76 CIEs, 1127 live FDEs), and all 22 Python tests pass.


## Xbox paced startup without automatic exit (2026-10-11)

Latest supplied paced-two-frame log passes both original signatures, runner
destruction, TLS cleanup and all 30 one-second hold checkpoints. Last line is
Returning from main; CRT will request dashboard launch. User's standing report
is shutdown with every log unless otherwise stated. That places this shutdown
after the final marker; it does not prove which destructor/CRT/dashboard action
causes it. Pacing success also does not establish why quiet slicing failed.

Next build mode --paced-startup runs 120 reference frames, frame 0 unchanged,
frames 1 onward sliced with native 1 ms delays. It retains 30 post-cleanup
heartbeats, closes the log and parks in kernel sleeps without returning from
main. Leave manually through Guide; no automatic return or poweroff request.
Output build/xbox360-game-paced-startup/default.xex, same roms/ and device log
xbox360-game-trace.log. No gameplay, GPU, audio or production scheduler claim.
Final source rebuilt: all 120 host ASan/UBSan frame hashes match the original
reference, ending at hash 23c8b9fbbbcea1b2. XEX mapped extent and unwind checks
pass (76 CIEs, 1126 live FDEs); 22 Python tests pass. Native pacing, parking
and manual Guide exit remain untested for this new build on the console.


## Xbox paced-slice and exit observation (2026-10-11)

Latest quiet-slice hardware log passes frame 0 then ends at Begin frame 1.
User clarifies shutdown accompanies supplied logs, including complete logs
unless explicitly stated otherwise. Earlier hash/cleanup PASS is not proof of
safe dashboard return. Newlib crt0 calls exit after main, and _exit requests
XamLoaderLaunchTitle(NULL, 0); this is not evidence of an intentional poweroff.

Added --paced-second-frame: same quiet instruction slices, native 1 ms delay
after each traced dispatch, then 30 one-second flushed heartbeats after TLS
cleanup and an explicit marker before returning from main. Host skips kernel
delays. Isolated output build/xbox360-game-paced-slice/default.xex; unchanged
ROM folder and xbox360-game-trace.log. This compares pacing without per-slice
USB writes and separates completion from CRT/dashboard return. Code layout
also changes, so success would not prove a timing root cause. Hardware pending.
Built successfully: host ASan/UBSan matches both reference frame hashes;
XEX mapped extent and unwind validation passes (76 CIEs, 1126 live FDEs).
All 22 Python tests pass. Native delays and exit observation require the next
console run; the host oracle deliberately does not exercise kernel APIs.


## Xbox quiet-slicing comparison (2026-10-11)

User's two-frame hardware trace passes both original hashes, runner destruction,
TLS cleanup and final PASS. Frame 1 succeeds when sliced with checkpoint I/O,
where the unsliced startup build shut down after frame 1 entry. This does not
yet establish the cause: logging latency, slicing and changed code generation
are potential differences, not confirmed explanations.

Added --quiet-second-frame. Retains unsliced frame 0 and identical frame 1
instruction budgets (first 64 single instructions, then 1024 batches), but
checkpoint() does no I/O. Only ordinary startup/frame results/cleanup messages
remain. Mode marker identifies quiet second-frame slicing. Separate output
build/xbox360-game-quiet-slice; device still writes xbox360-game-trace.log.
Same ROM folder and baseline runtime/generated objects. No crash-fix claim.
Host ASan/UBSan passed both reference signatures; mode marker verified and
checkpoint messages absent. XEX mapped extent/unwind validation passes
(76 CIEs / 1126 live FDEs / 52 discarded COMDATs). All 22 Python tests pass.
Quiet-slicing hardware run remains pending. Earlier diagnostic XEXs preserved.

## Xbox second-frame shutdown trace (2026-10-11)

Full startup hardware log passes frame 0 (ea78bb6a56c2fbc5) then stops at
Begin frame 1 ip=000016b4; user reports shutdown. No mismatch or final PASS.
Added --second-frame to build_game_trace.py, mutually exclusive with --startup.
Preserves the unsliced first frame; traces the first 64 instructions of frame
1 individually, then batches up to 1024, and stops after that frame. Same
hash/report/cleanup checkpoints and 32-bit-only formatting. Trace budgets are
relative to the frame's starting instruction count, not zero. Output isolated
in build/xbox360-game-second-frame; device log xbox360-game-trace.log, header
says two frames. Same six ROM files. Diagnosis remains open; do not call this
a shutdown fix or progress to GPU presentation before execution is stable.
Host ASan/UBSan passed both reference hashes; frame 1 ends at count 0x4a001,
IP 0x16b4, hash a0968dd6dbc7daef. XEX mapped extent and unwind checks pass
(76 CIEs / 1127 live FDEs / 52 discarded COMDATs). 22 Python tests pass.
Console execution remains pending. Previous binaries/ROMs preserved; no push.

## Xbox first-frame hardware pass; full startup retest (2026-10-11)

User reports no fatal error. Revision 2 xbox360-game-trace.log confirms frame
0 hash ea78bb6a56c2fbc5 at count 0x25001/IP 0x16b4, result written, runner
destroyed, TLS cleanup completed and final PASS on the USB/Freestyle 3 setup.
The 32-bit-only logging removes the observed failure in this one-frame test;
do not describe prior malformed count/IP fields as corrupted game state.

Next: build_game_trace.py --startup reuses the same baseline objects and
corrected runner, but disables instruction slicing/tracing and runs the full
120 startup frames. Host signatures must match all 120 original reference
values under ASan/UBSan before packaging. Output is isolated in
build/xbox360-game-startup; copy its default.xex beside the existing roms
folder and collect xbox360-game.log. No display/audio/input yet. This extends
coverage to initial TGP startup; it is not attract-mode or gameplay proof.
All 120 host ASan/UBSan signatures matched, ending at 11476993 instructions,
443 TGP instructions and hash 23c8b9fbbbcea1b2. 22 Python tests pass. Packaged
11214848-byte XEX passes mapped extent/unwind checks (76/1127/52); SHA256
63f254bb40dce7bdfa3b4d8960d44bed9f7abbc7a46e644e82d9c91683e4e563.
Full startup console test pending. First-frame trace binary preserved. No
commit/push or installed SDK changes.

## Xbox trace revision 2: diagnostic formatting (2026-10-10)

User's first trace reaches both vblank-end markers and Before frame signature,
then reports a fatal crash under Freestyle 3. Every displayed count equals
the phase string's linked address (e.g. 2191523985 = 0x82a00091 for Before
has_code), not a valid instruction count. The printed zero IP is likewise
unreliable. Do not infer corrupted CPU state from these formatted values.
The result printf also used two 64-bit arguments followed by a %s pointer;
argument misreading there could itself cause the fatal crash, but this has
not yet been confirmed on hardware.

Revision 2 removes all 64-bit printf arguments from the game probe, emitting
high/low unsigned 32-bit hexadecimal words instead. Adds flushed markers
after registers, main RAM, work RAM, screen and TGP signature stages, around
comparison/result formatting, runner destruction and TLS cleanup. Startup
identifies revision 2. Same one-frame test and unchanged reference hash;
still writes xbox360-game-trace.log. ROM files remain unchanged.
Host ASan/UBSan run passed all signature/report/destruction stages, preserving
ea78bb6a56c2fbc5 at count 0x25001/IP 0x16b4. All 22 Python tests pass. Packaged
11214848-byte XEX passes mapped-image/unwind validation (76/1127/52).
SHA256 cd4f7664026610cc0726d083c8dd910843cc5a92ab53090256bbb688a4da2bfe.
Revision 2 console run remains pending; no claim the fatal crash is resolved.

## Xbox first-frame shutdown investigation (2026-10-10)

User ran the real game probe from USB through Freestyle 3 (not Aurora).
xbox360-game.log verifies all six ROM images, constructs the board, and stops
at `Begin frame 0 ip=00000860`. User reports console shutdown, not a normal
dashboard return. No frame signature or completion marker exists. This is
not a hardware pass. Loading via game: worked; no evidence blames USB/FSD.
The first generated function's prologue reserves 7840 bytes; the PE requests
1 MiB stack reserve / 4 KiB commit. These are static observations, not proof
of a stack fault or actual kernel stack availability.

Added build_game_trace.py: retain the original generated/runtime objects,
record their hashes and baseline XEX hash, replace only the runner in a new
build/xbox360-game-trace directory. Trace one frame, stopping generated code
after each of its first 64 instructions and in 1024-instruction batches after
that. Flush count/IP checkpoints before has_code, before/after dispatch,
before/after both vblank phases and before hashing. No interpreter added.
The separate log is xbox360-game-trace.log beside the XEX; existing roms
folder remains usable. No screen/audio output expected. This is diagnostic
instrumentation, not a claimed shutdown fix.
Host trace passed ASan/UBSan and matches the original first-frame signature
ea78bb6a56c2fbc5 at 151553 instructions, IP 0x16b4. Leak checking alone was
disabled because this sandbox prevents its required ptrace operation; memory
and undefined-behaviour instrumentation stayed enabled. The original linked
first dispatch pointer equals the chunk_000 symbol (0x82010000); no static
pointer mismatch found. New XEX mapped extent/unwind validation passed:
76 CIEs, 1127 live FDEs, 52 discarded COMDATs. SHA256
4a818aafbfc4128cca1d9c4d5eb0fb5da955062a64f9e10006ca28664add6b5d.
22 Python tests and diff checks pass. Hardware run of trace XEX is pending.
No commit/push; existing failing XEX and installed SDK left untouched.

## Xbox real game startup probe (2026-10-10)

User's xbox360-render.log passes all 80000 texels and 16 frame hashes on
Trinity. Their xbox360-raster.ppm is byte-identical to the native reference
(`/tmp/daytona-render-reference.ppm`). This closes the synthetic CPU raster
hardware step, not GPU display or full game parity.

Added --game-probe: verified daytona93 import, normal m2recomp/m2tgprecomp
generation, 120-frame headless runner using actual M2Board, Cpu, free-run
Lockstep, generated i960/TGP, geometry and CPU video. No board_probe_guards,
interpreter, SDL, sound engine or transport. Scheduler copies GameLoop::probe
boundaries (1024/110000/40000/2048), with bounded frames and no-progress checks.
Compares fixed-byte-order register/RAM/screen/TGP-count signatures. This is a
portability test of existing generated semantics, not Xbox native-FP approval.

Host reference completed 120 frames: 11476993 i960 instructions, 443 TGP
instructions, final IP 0x12f0, signature 23c8b9fbbbcea1b2. The early startup
window is deliberate; it does not establish attract/race operation. No saves
loaded or written. Missing ROM input fails explicitly. Imported files are
size/hash-checked and staged as six .bin files (about 75 MiB) under ignored
build/xbox360-game/roms. Generated code and all ROM-derived output stay ignored.

M2_XBOX360_HEADLESS disables only unavailable IRQ-trace iostream loading and
threaded link waits: these operations throw if requested. Real free-run and
offline semantics remain; other targets do not define the macro. The default
host runtime rebuild and test_comm_board pass; 21 Xbox Python tests pass.
The larger executable initially failed the SDK linker's incorrect signed
24-bit byte-range check for PPC REL24. Same-tree ELF checks signed 26-bit
(LI is 24 bits in words). Added guarded COFF source patch (26-bit and aligned),
an isolated linker stage and an actual-object branch regression. Installed
SDK unchanged. TGP runs in the probe now also have a million-instruction cap.
REL24 regression passed: decoded forward/backward targets beyond 9 MiB;
overflow and unaligned destinations rejected. The SDK driver ignores --ld-path;
use -B with the isolated linker directory (confirmed with -###), not that flag.
Installed compiler/runtime fingerprint still matches. XEX built: 11214848
bytes, SHA256 b5e9a07da3710048a1cd5766d8a0fc15f095892690989f34b28803d1ec9a351e.
Image mapped extent and unwind checks pass: 76 CIEs, 1126 live FDEs, 52
discarded COMDAT entries. Existing /align warning remains. Hardware execution
of this game-startup build is NOT tested. 22 Python tests now pass, plus the
ASan/UBSan headless callback/offline/unsupported-feature guard regression.
The full 120-frame host game run also passed ASan/UBSan with no diagnostics;
all 120 generated signatures are byte-identical to the Release oracle.
No commit, push, SDK replacement or unrelated-platform package build performed.

Next hardware input: copy build/xbox360-game/default.xex AND its roms folder
to a new writable Aurora directory, launch, collect xbox360-game.log. No
onscreen output/audio/input expected; exits after bounded test. After matching
startup, extend real-code coverage and implement native framebuffer display.
Do not reintroduce the synthetic board guards into this target. Do not claim
complete gameplay, FP/MAME race parity, sound, or thread-safe libc++ yet.

## Xbox synthetic CPU rendering probe built (2026-10-10)

User's xbox360-board.log passed all ninth-stage checks and final completion on
Trinity. Advanced to guest-endian CPU texture sampling: scalar get_texel and
optimized read_texel_quad now use guest_load<uint32_t> instead of native word
dereference. VideoMem documents byte-backed guest-LE texture storage. Existing
little-endian behavior retained; no GPU/backend or game-generated code added.

New --render-probe builds a host pixel oracle and Xbox render_probe.cpp with
the actual src/runtime/raster.cpp, libc++ and TLS/exception support. Synthetic
byte-patterned sheets, palette/xlat/luma arrays; no ROM-derived data. 80000
packed-texel checks use an independent byte/nibble oracle. Sixteen nonblank
frames test textured quads/triangles, checker shading, mirror flags, both
sheets/origins and perspective. Fixed-byte-order hashes compared per frame;
the final framebuffer is written to xbox360-raster.ppm (496x384 P6 RGB).
Host reference PPM inspected: textured checker-shaded triangle, not blank.

Host Release, ASan/UBSan and M2_VITA_RENDER_OPT hashes agree for all 16 frames.
Existing test_vita_texel passes 2000000 footprints / 8000000 scalar samples.
All 21 Python tests pass. Build/xbox360-render/default.xex is 790528 bytes,
30 CIEs/715 live FDEs/5 discarded COMDAT records; mapped extent validated.
SHA256 4c35f1017f9ca392f4ee09fe177852a0db35aaf7939cebb7d6735b25e1c17adf.
Existing /align warning remains. Hardware rendering test pending: copy XEX
to new writable Aurora folder; collect xbox360-render.log and xbox360-raster.ppm.
No display output or input; intended dashboard return. No GPU screenshot,
game boot, full MAME parity or Xbox fast-path approval claimed.

Next: confirm hardware pixel hashes/image, integrate generated game code and
real scheduling without board test guards, and add native presentation. Test
images do not validate vblank/tile composition or actual game display lists.
No SDK replacement, commit or push; platform binaries otherwise not rebuilt.

## Xbox real board memory probe built (2026-10-10)

User's xbox360-fp.log passed extF80 aggregate ABI and all 57353 reference
checks/flags: hash 8b69a75c2515064d, zero known failures, final PASS on Trinity.

Added guest_endian.h and used it in m2_board fetch/read/write, TgpBoard ROM
word conversion and Geo polygon/texture ROM conversion. Little-endian path
retains memcpy; big-endian path explicitly converts guest bytes. No root
build changes. Rendering texture-RAM word views are not corrected by this.

New --board-probe builds real M2Board, Video, Geo, TgpBoard, CPU/raster support
with synthetic images (2 MiB program, 32 MiB main data, 8 MiB copro data,
256 KiB tables, minimal geometry ROM). Probe-only link guards THROW if scheduled
callbacks, linked cabinet operations or generated TGP execution are reached.
They are never game backends and must not enter a gameplay target. No vblank.

Checks pass on host ASan/UBSan (LSan disabled): construction, ROM fetch/order/
mirrors/protection, 3520 RAM cases in 11 mapped regions, word/byte/dword writes,
unaligned/page-crossing access, TGP ROM and masked buffer lanes, tile and
texture mirrors/packing, device-fetch exception, destruction. CPU control RAM
is a single page and intentionally excludes crossing its unmapped boundary.
Existing geometry tests pass 100000 clipping cases and 6000 batch states
(1411 kept polygons). All 21 Python tests pass. Physical board test pending.

build/xbox360-board/default.xex: 856064 bytes, 44 CIEs/890 live FDEs/30 discarded
COMDAT records; mapped image and board fetch/read/write coverage checked.
SHA256 e6abca3d8d4534edd116a842a85b417822a52ab9579c068161eb4fd5a6278599.
Existing /align linker warning. Copy only XEX to new writable Aurora directory
and collect xbox360-board.log. No ROM/image/input; intended dashboard return.

Next: confirm board hardware result, finish renderer texture-RAM word access
and integrate real generated code/scheduler without test guards. Network and
full thread-safe game runtime remain unfinished. No game boot/graphics/audio
claim, no SDK replacement or git push. Other platform binaries not rebuilt.

## Xbox big-endian SoftFloat probe built (2026-10-10)

User's seventh-stage runtime log passed every stage and final completion on
Trinity. Added --fp-probe, isolated softfloat/CMakeLists.txt/platform.h,
build_fp.py and synthetic fp_cases/oracle/probe sources. Uses unchanged pinned
SoftFloat and src/i960/fp.cpp. Shared root CMake and platform builds unchanged.
Xbox has no LITTLEENDIAN definition; host oracle uses native field order.
C and C++ share SOFTFLOAT_FAST_INT64 and compatible compiler TLS lowering.
Target compilation rejects non-big-endian extF80 layout; runtime also checks
f32_to_extF80's C-returned aggregate signExp/signif for 1.0.

57353 reference results and exception flags hashed byte-wise (no ABI padding):
9 known results plus 4096 special/random inputs across conversions, comparison
and scaling in four rounding modes, scale -500..500. Native host Release and
ASan/UBSan builds agree on 8b69a75c2515064d, zero known failures. LSan disabled.
21 existing Python tests pass. Generated oracle header is under ignored build/.
No exhaustive MAME check or Xbox fast-path validation claimed.

build/xbox360-fp/default.xex: 724992 bytes; mapped extent valid; 54 CIEs,
699 live FDEs, 5 discarded COMDAT records. Existing linker /align warning.
Copy only XEX into a new writable Aurora directory and retrieve xbox360-fp.log.
No ROM, image or input; intended dashboard return. Hardware FP test pending.

Next: collect checksum/ABI result, then M2Board endian-safe memory and generated
code integration. Audit found direct host memcpy loads/stores and texture-word
casts in m2_board.cpp; do not treat the synthetic byte bus test as board parity.
No board edits, graphics/audio, game boot, SDK replacement, commit or push.

## Xbox real runtime services probe (2026-10-10)

The user's xbox360-exception.log records typed catch, nested cleanup, rethrow,
catch-all and completion PASS on Trinity. This supersedes the pending status
for the sixth-stage exception probe below.

Added --runtime-probe to build_exception_probe.py. Builds exception-enabled
libc++ in the isolated ABI build and links unchanged src/runtime/cpu.cpp,
bounded compiler TLS, exception regression and runtime_probe.cpp. Synthetic
64 KiB byte bus, no ROM/interpreter/generated code. Tests reset state, 320
aligned/unaligned guest little-endian access/flags cases, call depths 1..12
with cache spill/restore, real Fatal string and libc++ out_of_range exceptions.
Each log checkpoint closes before proceeding. TLS cleanup follows exception
scope. Single thread; no general allocator/thread-runtime claim.

Host synthetic cases pass ASan/UBSan (LSan disabled), native host TLS and stub
kernel lifecycle only; they do not validate Xbox adapter integration. All 21
Python tests pass. Extended image validator requires CPU-service FDE coverage.
New build/xbox360-runtime/default.xex: 724992 bytes; full mapped extent checked;
30 CIEs, 715 live FDEs, 7 discarded duplicate COMDAT records. SHA256:
44a046d03074ed2e63157fd6bf61d70b35d909b10de4a39b1578e22140a0ac5d.
Existing /align linker warning remains. Hardware test pending: copy to a new
writable Aurora folder and collect xbox360-runtime.log. No picture or ROM;
intended dashboard return. No SDK replacement, shared-platform edits or push.

Next: collect integrated probe result, then endian-correct SoftFloat and
M2Board/generated-code integration. Root SoftFloat build explicitly assumes
LITTLEENDIAN=1; do not reuse that configuration unchanged for big-endian PPC.
No 3D rendering/audio/gameplay or full runtime boot is established.

## Xbox exception probe built; hardware test pending (2026-10-10)

Completed isolated patched Clang build and rebuilt libc++abi/libunwind with
DWARF exceptions and unwind tables enabled for both C and C++. In addition to
the type-reference crash, PowerPC COFF streamer omitted emitFrames(nullptr);
prepare_exceptions.py now reproduces that fix too. Runtime builds clean their
generated output and stamp the compiler hash to prevent stale objects.

build_exception_probe.py produces build/xbox360-exception/default.xex,
659456 bytes, SHA256:
dd163d97d567e50b3849f30e999f283c467a4d5b261ff10cd52e7dbcf2b2318b.
Link still warns about /align without /driver. check_exception_image.py checks
big-endian CIE/FDE structure, code/personality/LSDA pointers, frame markers and
coverage of main, cases, nested throw, __cxa_throw and _Unwind_RaiseException.
23 CIEs, 654 live FDEs, five zero-address discarded COMDAT records. The end
marker requires .eh_frame$ZZZ; plain .eh_frame placed it before archive frames.
All 20 Python tests pass, including mutation checks for frame validation.
Full mapped XEX size validated with the passing TLS packaging repair.

Copy only this XEX into a new writable Aurora folder and collect
xbox360-exception.log. START is closed before throwing; completion should
report typed catch/nested cleanup/rethrow/catch-all PASS. No picture, ROM or
input; intended dashboard return. Hardware exceptions are NOT tested yet.
No gameplay, shared-platform changes, installed SDK replacement or git push.

### Investigation history for this step

The user's replacement xbox360-tls.log confirms compiler TLS initialization,
alignment, worker/main isolation, both joins and 3 constructors/3 destructors
PASS on Trinity. This supersedes the hardware-pending notes below.

This step implements isolated single-threaded C++ exception support, not gameplay.
prepare_exceptions.py pins LLVM and guards unrelated edits; it replaces the
Xbox printer's erroneous XCOFF type-reference emission with the common
format-aware implementation and enables Xbox COFF aliases in libunwind.
build_exceptions.py builds a separate compiler/runtime without replacing SDK.
The compiler rebuild is in build/xbox360-llvm-eh; log /tmp/daytona-eh-build.log.
Runtime was rebuilt with that compiler before the exception XEX was made.

Preliminary old-compiler libc++abi/libunwind archives link with static frame
markers and the bounded aligned_alloc shim, but emit NO usable unwind tables.
Do not package build/xbox360-abi/exception-unverified.exe or use those archives
as evidence of exception support. The new builder/validator above replaces
this initial link-only experiment. Hardware execution still needs testing.

exception_cases.cpp passes 1000 typed-catch/cleanup/rethrow/catch-all cycles
on the host under ASan/UBSan. The allocation shim passes 320 allocation cases
plus invalid/unsupported alignment checks. These checks do not establish Xbox
exception support.

## Xbox TLS packaging repair after hardware crash (2026-10-10)

User reported Fatal Crash Intercepted and no xbox360-tls.log. The screenshot
does not locate the fault. Artifact inspection found a concrete packaging
defect: PE SizeOfImage=393216 but SynthXEX mapped basefile=327680. The final
.data section has VirtualSize 0x12534 with only 0x600 raw bytes. The pinned
mapper copies raw data and pads only its final raw page, omitting a required
64 KiB zero-filled page. This is a strong crash candidate, not hardware proof.

Added package_image.py: creates generated package.exe from smoke.exe with
zero-fill materialized into raw section data before invoking SynthXEX. RVAs
and initialized content are preserved. The original PE, external SDK and TLS
code are untouched; SynthXEX computes descriptors/hashes over the full image.
Post-package validation now requires the uncompressed mapped payload to match
PE SizeOfImage, in addition to XEX2/PE signatures. This is specific to the
pinned uncompressed SynthXEX output, not a generic compressed XEX validator.

Rebuilt build/xbox360-tls/default.xex: 397312 bytes including 4096-byte header,
393216-byte payload. SHA256:
f6fd35412e23ee24393636971d25932690ad76aed3ef75b218f34e44ffd9af23.
Four packaging fixture tests and eight orchestration tests pass: preserved
content/RVAs, zero fill, idempotence, malformed/overlap rejection and missing
XEX-page rejection. Hardware retest pending; do not claim the crash is fixed.

## Xbox compiler TLS adapter and probe (2026-10-10)

User's xbox360-thread.log records both worker joins/TLS checks, main isolation,
atomic total 32768, handle closes, TLS free and completion PASS on Trinity.
Advanced to actual compiler thread_local lowering with -femulated-tls; no
game emulation or shared-global substitution. New isolated tls_runtime.cpp
uses the proven kernel TLS API, no heap, bounded per-thread arenas and explicit
thread cleanup. Descriptor layout checked against pinned compiler-rt emutls.c.
Four contexts, 32 descriptors/destructors, 16 KiB per context, alignment <=256.
Static XEX only; no module unload, general std::thread integration or exceptions.

New --tls-probe builds build/xbox360-tls/default.xex (331776 bytes), SHA256
518d6ed300064f80a616d61b8dc30fcb5fe42d1afc0c0040c9a1cbe5bf32d330.
Compiler emits adapter/destructor registration calls; assembly has no tprel
access or .tls$ section. Tests cover zero/constant/dynamic initialization,
alignment/isolation/main preservation and three thread-exit destructors.
Worker rendezvous prevents false pointer-reuse failures. Joins are bounded;
no TLS shutdown under unjoined workers. Hardware execution is still pending.

Host tests: 200 worker lifecycles with isolation, alignment, reverse destructor
order, arena reuse, busy-shutdown refusal and reinitialization pass ASan/UBSan;
leak checking disabled due sandbox. Invalid size/alignment abort as intended.
Eight builder tests pass. Other platform/game sources and SDK unchanged.

Exception investigation: forced -fdwarf-exceptions -funwind-tables crashes in
PPCXbox360AsmPrinter::emitTTypeReference, which tries an XCOFF TOC section on
the PE/COFF path. This is in addition to the prior SjLj selection failure.
No compiler patch/unwinder has been implemented; adding libc++abi alone is
not sufficient. Next: collect xbox360-tls.log, implement the compiler's
exception emission/unwinder, then guest-endian/game/presentation integration.

## Xbox native thread/TLS gate (2026-10-10)

User's xbox360-cxx.log has PASS for all six checks and the completion marker:
static constructors, aligned allocation/overflow, vector/sort/span, string
copy, RAII and sqrt are now demonstrated on Trinity. No full ABI/game claim.

Inspected actual compiler output before attempting to link an unwinder.
Normal full-abi.obj has .text/.rdata/.data/.bss/.tls$ but no unwind tables;
PPCCOFFMCAsmInfo never selects an exception format. -fsjlj-exceptions fails
with Cannot select EH_SJLJ_SETUP_DISPATCH. Adding ABI libraries alone is not
enough. thread_local emits r2-relative calls@tprel, not a verified Xbox TLS
sequence. Do not execute it blindly or replace TLS with shared globals.

Added a separate C --thread-probe using SDK declarations and public Xenia
reference for return semantics. Two workers, 64 batches each of 256 atomic
increments, distinct kernel TLS sentinels, main TLS preservation, bounded
joins and handle cleanup. No allocation/stdio in workers; SDK malloc remains
unsafe for concurrent use. Timeout path retains static worker contexts/TLS
until process teardown. Not std::thread/compiler TLS/C++ exception support.
Built build/xbox360-thread/default.xex; console execution remains pending.
SHA256: c4519a179033fea34ac236afc4edf56f0aba7d78a0a05c983d255b1c5dbfebad.
Import inspection confirms ExCreateThread, ExTerminateThread, KeTls APIs and
NtWaitForSingleObjectEx; disassembly includes lwarx/stwcx and memory barriers.
Seven builder tests pass. Existing platform sources and SDK are unchanged.
Next: retrieve xbox360-thread.log; compiler exception lowering/unwinder and
compiler TLS adaptation still need implementation before game integration.

## Xbox 360 hardware evidence and restricted C++ probe (2026-10-10)

User supplied xbox360-boot.log: endian PASS and filesystem write/read-back
PASS. xbox360-input.log: slot zero status 0 throughout recorded polling,
steering 32..224 with neutral 128, both pedals 32..224, changing button masks;
slots 1..3 status 1167. These establish native boot and input on the user's
Trinity, not graphics/gameplay, hotplug or a confirmed dashboard return.

Built an isolated static libc++ from the already-pinned LLVM source using
build_libcxx.py. No host C++ library, SDK source edit, or shared-platform
change. Added --cxx-probe packaging: build/xbox360-cxx/default.xex, 397312
bytes, PowerPC/32-bit Xbox PE plus XEX2 checked. Hardware testing pending.
SHA256: 5027068a4f24affaf618ce591d25f2726af9a5e25547be95d9ca33553559b0c8.
Probe logs constructors, allocation alignment/overflow, vector/sort/span,
heap strings, RAII and math; includes a flushed final completion marker.

Failures/workarounds: Generic CMake produced two libc++.a Ninja rules even
with shared disabled (excluded shared target); distinct shared output fixes
this. Function sections produced undefined symbols in object files, including
std::__throw_bad_alloc; disabling function/data sections for libc++ resolves
the restricted link. Later -isystem libc++ headers lost ordering to the SDK's
C headers; -I fixes wrapper ordering. Upstream aligned allocation references
missing _memalign_r; probe-only new/delete retain original malloc pointers
for aligned free, check overflow, and abort on throwing allocation failure.
This is not thread-safe and does not implement new-handler semantics.

Six builder tests and 40 aligned allocation cases plus overflow/array
checks pass on host with UBSan. Restricted library and probe also build/package
from a fresh separate directory using the checked-in scripts. The full
exception/TLS probe compiles with the generated headers but still cannot link
__cxa_allocate_exception, __cxa_throw, catch/RTTI and allocation symbols.
The restricted library explicitly disables exceptions/RTTI/threads; do not
mistake it for the full C++ runtime or remove shared game exceptions to fit.
Next: collect xbox360-cxx.log, then complete/test ABI/unwinding/TLS/threads,
guest endian parity, presentation/audio and game integration in that order.

## Xbox 360 initial audit and boot probe (2026-10-10)

Created xbox360 from the current clean main; no other branch was changed.
Audited repository architecture and pinned OpenXeChain buildscript/LLVM,
newlib, xecorelib and SynthXEX sources. Plan, evidence and deployment gates
are in platform/xbox360/README.md. The SDK is not a ready SDL replacement:
C++ standard-library hooks are empty and no C++ library is built; controller
and audio import names do not establish tested typed interfaces; Vd headers
include unknown arguments. Do not invent these APIs or treat generic PowerPC
ELF as a native Aurora XEX.

Added a ROM-free C boot/filesystem/endian probe, a fail-fast isolated builder,
host endian tests and a manual experimental CI workflow. The shared runtime
still requires a systematic endianness conversion (board memcpy accesses,
CPU register spills, TGP words, texture casts and SoftFloat layout). No global
LITTLEENDIAN toggle or existing platform behavior was changed. Full game,
rendering, controller and audio game integration are not implemented yet.
A separate controller probe uses the public Xenia-referenced XAM signature;
packet decoding/ADC mapping has exhaustive host tests. Polling and hotplug
remain hardware-unverified. Keep this separate from the first boot test.

Real Trinity launch, marker persistence and dashboard return require hardware.
Host Clang 23 initially rejected LLVM's optional benchmark __COUNTER__ macro
with -Werror,-Wc2y-extensions. Resume by preconfiguring the LLVM cache with
LLVM_INCLUDE_BENCHMARKS=OFF and LLVM_INCLUDE_TESTS=OFF; do not change target
code or weaken project floating-point settings. CI/instructions include this.

The next upstream failure was compiler-rt finding /usr/lib/cmake/llvm and
trying to import host shared libraries into Generic/Xbox. The checked-in
finish_toolchain.py completes this stage with LLVM_RUNTIMES_BUILD=ON,
source-tree CMake modules and static compile checks, then installs SynthXEX.
This completion script passed from a fresh separate build directory. LLVM,
newlib and xecorelib themselves were built by the pinned upstream script;
no upstream source patches were needed. The upstream /align-without-/driver
linker warning remains and is not proof either way of console compatibility.

Both C probes build and package: build/xbox360-smoke/default.xex and
build/xbox360-input/default.xex, each 266240 bytes. PE inspection reports
32-bit PowerPC, machine 0x1f2, Xbox subsystem 0x0e; input imports include
XamInputGetState. SynthXEX validates the intermediate PE; XEX2 magic checked.
These are structure/build checks, NOT a hardware boot claim.
SHA-256 (2026-10-10):
- boot: a77be4a97626ec0f2be0aadeea98a9d8c33cf72918c99dad201bd7f1541e8bb6
- input: 6e9d5f01ff99a4f4a717288dd33ca893a5f94d3aaffecc68408f878ca50eda65

Host validation: four Python builder tests, 262144 guest endian cases and
65536 steering cases plus packet/pedal checks pass; workflow YAML parses.
Address/UB checks pass with LeakSanitizer disabled because this sandbox's
ptrace environment prevents leak checking. No console or hosted CI run.
The actual target C++20 probe fails at <vector> not found, confirming the
standard-library gate. Do not claim gameplay or attempt to link host C++
libraries. Next: run the boot probe on Trinity, then the separate input probe;
port/validate the C++ ABI/runtime before integrating GameLoop. Native video
presentation/audio, full guest-endian conversion and memory telemetry remain.

## Mobile upstream rebase and GPU startup diagnostics (2026-10-07)

Rebased mobile onto original alphanu1 upstream/main 0435a46; recovery branch
backup/mobile-before-gpu-20261007. Main/Vita/PSP refs are not modified.
The Xiaomi Mali-G715 report exits normally from SDL_main after GPU creation
fails, not an established native crash. API version alone cannot establish
support; no device feature dump or physical phone is available here.

Shared SDL_GPU creation now uses properties to opt out of unused shader clip
distances, indirect-draw first-instance and anisotropy. Audited game shaders,
samplers and pinned ImGui backend for usage. Depth clamping remains required:
the current pipelines disable depth clipping, so merely disabling that device
feature would be invalid. No shader fork or external dependency edits.
fail() now snapshots the SDL error, logs it and opens an SDL native message
box; dialog failure is logged too. Startup lists compiled GPU backends.
These frontend changes follow the design's Platform layer section.

Mock regression tests cover requested features, property/device failures,
cleanup and error-message preservation even when logging changes SDL's error.
Host app objects and mobile main syntax checks pass; GPU startup, pacing and
TestHold tests pass. Xiaomi runtime compatibility and dialog appearance are
not hardware-verified. Android build initially required matching ANDROID_HOME
and ANDROID_SDK_ROOT; mismatched paths are a local configuration issue.
Android assembleDebug succeeds with both SDK variables pointing to
/home/boucy/Android/Sdk. The arm64 APK passes ZIP integrity and apksigner
verification, contains libmain.so/libSDL3.so, and contains no ROM archives.
Touch-control and mobile ROM-path regression tests also pass.

## Original upstream and platform test-menu integration (2026-10-06)

Merged alphanu1/daytona-arcade-recomp main 1877da9 into the fork, not merely
the fork's origin/main. Recovery branch: backup/main-before-alphanu1-20261006.
Upstream adds the shared TestHold helper and launcher action. Mobile keeps
that shared action with wrapped help text on small displays; Vita exposes
the same one-shot action in its ImGui options, retains physical Test/Service
bindings and does not persist the request. Both ROM versions wait until
frame 240, hold Test for 173 game frames and release, per upstream tests.
Resetting an armed Vita game restarts the full hold; Reset Defaults cancels it.
Inputs and Platform layer sections govern this frontend-only integration.

All 15 targeted tests (13 Vita, pacing and TestHold) pass, as do standalone
touch controls and 13 Android-path plus two desktop ROM-file cases. Desktop
app objects build and mobile launcher/main syntax checks pass with M2_MOBILE.
Vita main_gpu.cpp cross-compiles for both the 1993 and Revision A targets.
Full ROM cross-rebuilding was stopped deliberately in favor of targeted
frontend compilation; no new VPK/APK/IPA or physical device validation is
claimed for this update. Vita and mobile branches are advanced to the
integrated main; PSP untouched.

## Vita and mobile integration into main (2026-10-06)

Rebased psvita-native-frontend and mobile onto fetched origin/main 91560cf.
Mobile's touch controls and iOS file picker were already ancestors of main;
its rebase therefore fast-forwarded without replaying commits. Vita replayed
13 commits, retaining the ImGui launcher, both ROM revisions and platform
settings. The only conflict was this handoff: both upstream licence notes
and platform history were retained. No implementation conflicts occurred.
Recovery refs: backup/vita-before-main-merge-20261006 and
backup/mobile-before-main-merge-20261006. PSP is untouched.

This merge integrates the rebased Vita work into main; mobile is already
included. Publishing uses an explicit lease on the old GitHub Vita tip,
with main and mobile updated by fast-forward in the same atomic push.
Device builds and hardware runtime testing are not repeated by this merge.

Validation: desktop app objects and the selected test targets build; all
13 Vita tests plus app_pacing pass. Standalone mobile touch controls and
Android/desktop ROM-file tests pass. The broad all-target build stopped at
test_fp with unresolved SoftFloat helper symbols in this empty-build-type
configuration; no floating-point implementation changes were made. Mobile
tests are standalone programs/scripts, not named CMake build targets.

## Vita main rebase and ImGui launcher (2026-10-06)

Rebased psvita-native-frontend onto fetched origin/main 50d6638. Recovery
branch backup/vita-before-imgui-20261006 retains the pre-rebase history.
Main and PSP branches are not modified by this work; no Vita push performed.

Replaced the GXM frontend's bitmap menu with pinned upstream Dear ImGui,
rendered through the existing vita2d context. All 28 Vita options, dual-ROM
launching, link settings, physical controls and saved settings remain.
Front touch operates rows, adjustment buttons and the scrolling settings
panel; D-pad selection scrolls into view. UI work runs only in the launcher,
pause menu and loading screen. No additional gameplay display buffering.
The adapter supports the uniform-tint primitives used here, not arbitrary
per-vertex colour gradients. Geometry fringe AA is disabled; font AA remains.

Rebase integration preserves the Vita native-sized backdrop stretch path,
widescreen CPU tile fallback and foreground caches, while adapting the HUD
polygon helper to upstream panel detection. Non-Vita external-renderer
margin handling is retained. Host desktop app compiles; all 13 Vita host
tests pass, including real ImGui draw generation against a mocked backend,
pool exhaustion and GPU shutdown synchronization. These checks do not prove
physical Vita rendering, touch behaviour or gameplay performance.
The Vita ImGui target disables its unused desktop shell-opening handler,
which otherwise links unavailable execvp/waitpid functions from VitaSDK.

Both VitaSDK GPU builds pass. Final dual-ROM package:
build/vita-enhancements/daytona_vita.vpk (01.24, Daytona Recomp ImGui).
ZIP integrity and ARM ELF checks pass; the bundled daytona.self SHA256
matches build/vita-revision-a/eboot.bin. Archive contains executables and
licenses only, no ROM archives. Physical Vita installation remains untested.


## Mobile touch controls and latest main (2026-10-06)

## Vita Revision A link and dual-ROM launcher (2026-10-03)

Rebased psvita-native-frontend with --rebase-merges onto fetched origin/main
9ad266b (three-computer link validation). Recovery branch:
backup/vita-before-link-20261003. Merge conflicts combined enhance and
comm_board in Vita runtime; preserved Vita wide margins and foreground
cache invalidation while accepting upstream scene detection. Main and PSP
branches are unchanged; nothing pushed.

User supplied roms/daytona.zip alongside daytona93.zip. Revision A import
passes CRC validation; its i960/TGP/sound code is separately generated under
build/revision-a-host, never committed. GXM build selects DAYTONA_VITA_ROMSET;
the daytona93 VPK can bundle Revision A's SELF as app0:daytona.self.
Options ROM selector + Start/Reset replaces the process using LoadExec.
Each executable uses its own ux0:data/<set>/<set>.zip, vita.cfg, EEPROM and
backup RAM; existing 1993 saves are not repurposed as Revision A saves.

Vita SceNet IPv4 transport implements the shared TCP ring protocol with
nonblocking connect/accept/read/write, partial-send retention, a 64 KiB
bounded queue, reconnect delay and two-second no-progress timeout. Network
modules/heap are owned only where initialized here. Options expose enable,
four next-IP octets, local/next ports and optional frame sync (off by default).
Reset applies changes; pause status shows local IP, RX/TX and cabinet ID.
1993 link requests are rejected explicitly; Revision A is required on peers.
Master/slave, unique car numbers and matching cabinet/region settings remain
the game's test-menu choices. Native audio now tolerates unsupported effects
as upstream does (linked vibrato omitted), while invalid data still faults.

The host integration uses the real Vita transport through a narrow POSIX
SceNet shim against desktop TcpLink: numbering and data both directions,
37-byte partial sends, stalled-peer loss, reconnect and queue overflow pass.
This is not evidence of Vita Wi-Fi, executable switching or a hardware race.
Those remain the next device checks. CPU500/core options, road renderer,
wide CPU tiles and removal of the buffer selector remain preserved.
Address/undefined sanitizer execution of the socket integration also passes.
Host suite after rebase: 24 tests pass, two optional Lua tests skip.
Both ARM release executables and SELF/VPK builds pass. Final dual package:
build/daytona-vita-dual-rom-link.vpk. Archive checks pass; bundled eboot.bin
and daytona.self match their respective build outputs. The archive contains
only executables, SFO and license documents, no ZIPs or extracted ROM assets.
Build directories: build/vita-enhancements (1993, existing generated input),
build/vita-revision-a (fresh Revision A generation). Both were incrementally
rebuilt after final frontend edits, then bundled in that order.

## Remove configurable Vita display buffering (2026-10-02)

User reports 30 FPS and requests removal of single/double/triple settings.
Removed the menu/config field, physical ring adapter and its obsolete tests.
Old gpu_buffers keys are ignored and disappear on the next settings save.
The same pinned libvita2d now builds without source injection, retaining
upstream's normal three-surface queue and display synchronization. This
removes custom buffering, not the framebuffer storage needed for scanout.

Inspection found no explicit 30 FPS cap: FrameClock retains fractional time
at native 57.524 Hz; the display callback waits one vblank, not two.
Do not claim the reported slowdown is proven caused by buffer count, or
force game logic to 60 Hz. Audio pacing, CPU/core options, renderer and
wide tile fallback are unchanged. Hardware speed remains unverified.
Package: build/daytona-vita-standard-presentation.vpk.
Validation: release cross-build and archive check pass; ELF has upstream
vita2d_swap_buffers and no custom daytona_vita2d symbols. Host CTest:
21 passes and two optional Lua skips. New one-step GXM clock regression
produces 5752 game steps over 6000 simulated 60 Hz display ticks (100 s).
Deleted adapter/test sources remain recoverable in Git history.

## Optional Vita CPU clock and fourth core (2026-10-02)

GXM options now include CPU 500 MHz and a persisted fourth_core switch.
Defaults remain 333 MHz and fourth core off. CPU requests are read back;
rejected/ineffective 500 MHz requests fall back to a 444 MHz request, with
actual frequency shown in options. An overclock plugin/profile may override
application requests; selecting 500 alone is not proof it is active.

Main probes USER_ALL | SYSTEM affinity and falls back to USER_ALL on failure.
Reference sound jobs and both audio callback implementations adopt the shared
mask once per change, with callback caches reset when reopening devices.
No kernel patch/dependency is installed. A working core-unlock plugin is
required. Existing threads can use the additional core; no game-loop
parallelization or audio clock/pacing change is introduced.

Host fallback tests and audio worker/lifecycle tests pass; complete CTest
suite: 22 passes, two optional Lua skips. Vita release cross-build and VPK
archive validation pass. Package: build/daytona-vita-500mhz-fourth-core.vpk.
Actual plugin acceptance, stability and performance require hardware testing.
Wide CPU tile fallback, GPU roads and double-buffer default are unchanged.
Main and PSP remain untouched; no push.

## Wide tile recovery after hardware slowdown (2026-10-02)

User reports moving tile composition onto GXM made the game slower. Disable
the explicit wide GPU tile capability in the Vita frontend: widescreen now
uses the existing CPU backdrop/foreground fallback. Original-aspect GPU tiles,
GPU 3D, the road subdivision change, audio, clocks, physical double-buffer
default and main3044f3b remain unchanged. No additional performance hypothesis
is presented as established; the earlier CPU-only timing excluded GPU cost.

This isolates wide tile composition from the road change, which also can add
vertex work. If this build remains slow, measure/subtract that separately;
do not advertise moving work to GXM as automatically faster. Hardware result
for this comparison build remains pending. Main and PSP untouched; no push.
Package:build/daytona-vita-wide-tile-recovery.vpk.
Validation: VitaSDK build and21 CTest passes (2 optional Lua skips). The
fallback composition and mode transitions are covered by renderer tests.

## Rebase onto GitHub main and GPU tile/road follow-up (2026-10-02)

Fetched origin/main3044f3b and rebased psvita-native-frontend with merge history
retained. Backup:backup/vita-before-main-20261002 at20becae. A preliminary flat
rebase was aborted before continuing with rebase-merges to preserve Vita merge
content. Resolved Video API conflicts by keeping desktop snapshots/instance
tracking and preserving Vita widescreen, plus the renamed HUD copy destination
parameter. Main and PSP branches are untouched; no remote push. The obsolete
Vita frame-skip helper/docs from an old merge were not reinstated; replacement
docs describe the physical GPU buffers (double default).

New main renders tiles on GPU except for per-item HUD relocation, which still
uses CPU foreground composition. Vita now matches that split: GXM composes
centred foreground as well as backdrop, skips CPU pixel composition/uploads,
and retains dirty state for transitions back to the relocated HUD fallback.
CPU tile decode/cache work remains. Main's SDL_GPU shaders are not Vita GXM
binaries; they are retained for desktop, not falsely advertised as a direct
Vita shader port. Audio timing, game cadence and double buffering unchanged.

Main's per-fragment reciprocal-depth UV correction confirms the difference
from Vita's affine tessellation. Add an edge-midpoint texture-error criterion
alongside the existing depth/span minimum subdivision. A synthetic shallow
road triangle goes from46.545 to0.925texels midpoint error, at subdivision8
instead of1. Initial test wrongly expected0.5 at that cap; corrected to check
the measured sub-texel result. The0.5target is not a guaranteed bound, especially
at the eight-way cap or when pool pressure lowers subdivision. This can add
vertex work. No homogeneous matrix or untested shader replacement is used.

Tests exercise scroll-dirty GPU foreground skipping, fallback transitions,
HUD placement, source UV/stretch separation and25440 tessellation cases.
Hardware road appearance, GPU timing and overall speed remain unverified.

Validation: full host build,21 CTest passes with2 optional Lua skips, and
ASan/UBSan renderer contracts pass. The first dirty-transition test omitted
tile_memory_w under write tracking; it now uses the real board notification.
Desktop SDL_GPU Vulkan smoke tests complete120 and1200frames at16:9; the latter
with HUD-edge option enabled. This is not Vita rendering or full-race parity.
VitaSDK build and archive checks pass. Package:
build/daytona-vita-main-gpu-tiles.vpk
SHA256:50dde138308c20680cf6b5a4f9a8ba92abc5c84cb4aa9fb8a4104c90e5a3728b.
Frontend options, controls, audio and physical buffer adapter compare unchanged
against the pre-rebase backup. Source tree is ready for device testing; no push.

## Widescreen GPU backdrop optimization (2026-10-01)

Hardware feedback: all physical buffer choices still slower in widescreen.
Buffer count was not a cure for the extra rendering work. The wide path still
CPU-composed scrolling backgrounds, unlike original aspect's GXM tiles.

Add an explicit GPU-background capability enabled only by Vita's GPU frontend.
Wide screen_update skips CPU background composition/copies; GXM uses the
existing System24 tile upload and composition around unchanged 3D polygons.
Only background destination x changes for stretch, never UVs or 3D projection.
CPU foreground and per-item HUD relocation remain intact. Non-stretched side
margins clear to palette pen0 as in the original GPU path. The old CPU backdrop
path remains when capability is off; normal desktop/PSP behavior is unchanged.
Double GPU buffers remain default; no frame skipping, audio or clock changes.

A synthetic host test (1000 scrolling updates,margin93,empty geometry,constant
character pixels,changing hscroll) measured526.78ms before and238.81ms after
for CPU screen_update, about55percent less. Commands/source are under ignored
build/vita-wide-bench-new.cpp and /tmp/vita-wide-bench.cpp. This excludes GPU
cost and does not establish hardware FPS, visual fidelity or a full-speed game.
Prior homogeneous-WVP failure is not reintroduced: no draw_polygons changes,
shader changes, custom matrix, or HUD algorithm changes.

Host renderer contracts cover margins59/93/200, centred/stretch coordinates,
unchanged UVs and HUD pixels, no CPU backdrop generation, and one CPU layer
draw instead of two. Full host build and21 CTest passes (2 optional Lua skips),
ASan/UBSan renderer contracts, VitaSDK build and VPK archive checks pass.
Package:build/daytona-vita-wide-gpu-background.vpk
SHA256:400df3856a523876c0686cd47a8aac79c7c555b3b0e3d247760fb8e5ed36cb1e.
Hardware rendering and audio checks remain necessary. Main/PSP untouched;
no remote push.


## Vita physical GPU display buffers (2026-10-01)

User requested actual Vita GPU buffering separate from main's software draw
mode, with double as default. Options now persists gpu_buffers1..3; default
and Reset Defaults select2. Old draw_mode remains ignored. No frame skipping,
clock catch-up, audio backend, or polygon-renderer changes are reintroduced.

The pinned public MIT libvita2d source is built privately in the build tree.
display_buffers.inc changes the active GXM surface ring, not board cadence.
Double/triple retain normal display-queue sync; single finishes GPU work and
directly presents the sole surface without queuing identical old/new sync
objects. It may tear during rendering and is not promised faster. Switching
drains GPU and display work, copies the latest frame to surface0 when needed,
waits for scanout, then resets front/back indices. All three allocations stay
alive to allow switching; this does not reclaim framebuffer RAM.

Source archive hash verified. Initial build caught upstream JPEG ceil missing
math.h with the current compiler; target-only forced include resolves it.
All seven pinned shaders' extracted binaries match the installed package
exactly (object metadata hashes differ). No shader compiler used.
The design and dependency records now describe physical buffers rather than
the withdrawn frame-skip options; upstream MIT license is packaged.

Validation: adapter tests cover all mode transitions, VSync on/off, invalid
counts, uninitialized/in-scene/system-app rejection and GPU-before-display
ordering. ASan/UBSan pass;21 CTest passes,2 optional Lua skips. VitaSDK cross-build,
linked buffer-control symbols and VPK archive checks pass. Package:
build/daytona-vita-gpu-buffers.vpk. Hardware switching,
tearing, frame pacing and audio remain unverified. Main/PSP branches untouched.


## Vita pre-draw-mode pacing recovery (2026-10-01)

Hardware feedback after9790e57: game and audio still lag since draw modes.
The synthetic debt test did not establish the cause of native audio stutter.
Back out the Vita draw-mode UI, config loading and presentation gate, and the
follow-up retained-debt clock. main_gpu.cpp is identical to6a6fdc8 apart from
the PACING RECOVERY menu title; controls and input tests match that baseline.
This restores the previous VSync-paced presentation on every frontend loop,
not just when a board frame is due. Saved draw_mode values are ignored and
removed on the next settings save; other preferences are preserved.
Shared main frame-skip support remains merged but defaults to zero and is
not enabled by Vita. Native audio remains device-clocked per the design
document Audio section; neither audio backend nor GPU geometry was changed.
Do not re-propose frame skipping as physical single/double buffering.

Validation: source comparison to6a6fdc8,20 CTest passes,2 optional Lua skips,
VitaSDK cross-build and VPK archive validation. Installable recovery package:
build/daytona-vita-pacing-recovery.vpk. Hardware playback remains unverified;
this is a controlled rollback, not a claim of a measured hardware speedup.
Main and PSP branches untouched. No remote push.


## Vita audio pacing follow-up (2026-10-01)

Hardware report: audio stutters with draw-mode selection. A deterministic
37-second alternating 35 ms / 2 ms host-timing test exposes the one-step
clock discarding whole-frame debt:1114 board/audio steps instead of2128.
Vita GPU frontend now retains at most four frames of debt, still executing
only one board frame per loop. Cheap skipped frames can recover that debt;
no multi-step live-geometry submission or renderer changes are introduced.
Pause/reset clears debt; sustained overload remains bounded. Other frontends
keep the existing FrameClock policy. Audio dispatch stays outside draw gating.

The design document Audio section requires device-clocked native playback.
That backend already has it and is unchanged. This finding explains reference
sample starvation and late main-board commands, not proven native callback
underruns. Do not claim this reproduces or resolves every hardware crackle;
if native music still stutters, capture callback timing and active engine on
the device next rather than increasing latency or changing audio fidelity.

Validation: regression recovers2128 steps, bounded overload/reset tests pass;
20 CTest passes,2 optional Lua skips; public VitaSDK cross-build passes.
Package:build/daytona-vita-audio-pacing.vpk. Hardware listening remains pending.
Main and PSP branches unchanged; no remote push.


## Vita draw modes from updated main (2026-10-01)

Merged origin/main931504e05f4cd471a207c78b275f48e6ce77cc4c into the Vita branch.
Upstream calls frame-skip presets Double Buffered / Single Buffered / Every
Third Frame. They mean draw every1/2/3board frames, NOT physical buffer counts.
The earlier uncommitted buffer-library drafts were moved to ignored
build/paused-buffer-drafts; they are not compiled or shipped. The installed
libvita2d, framebuffer ring and VSync remain unchanged.

Vita Options exposes those three modes, persists draw_mode0..2 (default0),
clamps loaded values and applies on resume. GPU submission/clear/swap is gated
by the same pre-step board frame counter used by M2Board::vblank_end.
No stale live geometry is redrawn on skipped frames: retain the last displayed
frame instead. The clock is capped to one board step per frontend iteration.
Board execution, input and reference/native audio dispatch stay outside the
presentation gate. Menu drawing remains available; zero-step iterations avoid
duplicate scene submission and retain the existing idle delay. SIM FPS label
makes clear the counter measures simulation, not presentation cadence.
The polygon renderer and perspective vertices are unchanged from Wide2.

Validation:20CTest passes,2optional Lua skips. Draw cadence tests sweep invalid
and valid modes over600board frames (600/300/200draws). Public VitaSDK build
and VPK archive checks pass. Three6000frame race_basic headless replays show
identical196665345i960,223429779TGP,12050interrupts,3636sound-command bytes,
and7843660868000instructions. Host times35.18/19.45/14.24seconds are not Vita
FPS. Last-picture hashes differ as expected because skipped modes retain an
earlier frame:ad67233983ea8808 /04b9cd7b8fe2cdda /0504a052c927e9ff.

Artifact: build/daytona-vita-draw-modes.vpk
SHA256c02ffa1702a682062c991cbf569dabafcbe8bd22d055540254fd6281fc8833b1.
Menu title DRAW MODES. LoggingOFF. No hardware result yet.
Main and PSP branches untouched; only the local Vita branch is updated.
Next: compare simulation pace, audio and displayed motion across presets on
real Vita. Lower draw frequency can reduce render cost but also makes motion
less smooth; do not advertise these presets as actual buffer-count changes.


## Wide2: merge GitHub main and Vita options (2026-10-01)

Rebased mobile again onto origin/main 10c85cb before completing touch input.
Kept upstream pacing.cpp and Pacer alongside mobile controller startup, iOS
Files import and screen fixes. Backup: backup/mobile-before-main-touch-20261006;
the touch-work stash remains as an additional recovery copy. No branch pushed.

Shared mobile-only touch overlay: analogue horizontal steering, gas/brake,
sequential gears, four views, coin/start and launcher menu. Raw SDL fingers
capture controls independently (device and finger IDs), not ImGui's single
mouse pointer. Short taps survive until a game frame; focus loss, cancellation,
menu and safe-area changes release input. Existing physical controls merge
with touch through Controls; desktop receives zero touch input by default.
This follows the design's Platform layer & build boundary; runtime/shaders
and upstream pacing remain unchanged. See mobile handoff for validation.

## Mobile rebase onto remote main (2026-10-06)

Rebased mobile onto origin/main 6817bff, preserving Android document imports,
iOS Files import, screen sizing and unsigned packaging. CMake conflict
resolution keeps upstream link/force-feedback sources and Windows socket
libraries alongside mobile SDL targets. Generated-source paths combine
M2_GEN_ROOT with upstream M2_ROMSET. Mobile startup gamepad enumeration now
uses the upstream Devices owner. Initialize the link address before Android's
stale-URI early return. The previous mobile tip is retained at
backup/mobile-before-main-20261006. Main and the other platform branches are
unchanged. Existing IPA files predate this rebase and must not be presented as
rebuilt from it.
Validation: desktop daytona_app compilation passes; 13 Android-path import
tests and 2 desktop-path tests pass with the host SDL shim. Remote main is an
ancestor of the rebased tip. iOS/Android device builds are not rerun here.

## iOS launcher and Files picker (2026-10-06)

Replaced the unsupported SDL iOS dialog with a UIKit document import delegate,
keeping a separate private copy for each selection before the existing ROM
validation. Enabled modern full-screen launch sizing, safe-area placement,
logical-point ImGui styling, text wrapping, a visible scrollbar and blank-space
drag scrolling. Picker errors are shown beside the ROM field. Changes follow
the design's thin platform layer; runtime and shaders are unchanged.

Device Release IPA and simulator build succeed. The simulator cannot provide
visual verification: SDL_CreateGPUDevice reports that its device does not meet
SDL_GPU Metal hardware requirements; the resulting black screen is not UI
validation. Native Files selection and layout need another physical-device
test. See platform/mobile/HANDOFF.md. Do not claim successful picker import or
screen fit based on compilation alone.

## Mobile iOS packaging (2026-10-06)

The iOS build script supports `UNSIGNED=1` for AltStore: disable Xcode
signing, stage the device app under `Payload/daytona.app`, and produce
`build/ios/Daytona-unsigned.ipa`. The bundle template now explicitly supplies
CFBundleExecutable and the APPL package type. Custom build and generated-source
paths are normalized before packaging; argument handling remains compatible
with macOS Bash 3.2. No ROM archives or generated sources are committed.
This follows the design document's Platform layer & build and ROM handling
sections: the shared runtime/renderer is unchanged. See the mobile handoff
for build validation and device-test status.

## Recovery 1 after hardware regression (2026-10-01)

IMG_2856 shows the c3ec891 package losing most textured scene geometry: road
and car surfaces missing, while tiles and some solid/checker geometry remain.
This overrides the host-test success recorded below. The exact GXM failure
mechanism has not been isolated; the host uniform/projection model did not
execute the installed shader and was insufficient to validate the change.

Restored gpu_fast.cpp, perspective_vertices.h, video.cpp/h and corresponding
renderer tests byte-for-byte to1ca8cb3 (git diff --exit-code verified).
Both homogeneous-WVP and wide GPU-tile changes are withdrawn, rather than
shipping another unverified matrix guess. Steering curves, their controls
tests, saved settings and existing Test/Service bindings remain.
Menu identifies DAYTONA RECOMP - RECOVERY 1. Main and PSP untouched.

Public VitaSDK cross-build and VPK archive checks pass. Input tests pass;
restored renderer ASan/UBSan contracts pass, including painter order, checker
parity, wrap/mirror addressing, allocation ownership and shutdown.
Artifact: build/daytona-vita-recovery-1.vpk
SHA256825368c373f9c52dd00329ff16fc9d9e08aed68c0dfa614efe3eed584693e0a5.
Logging remains off. No hardware recovery result has been observed yet.

Road wobble and wide-mode cost remain as in the earlier working renderer.
Next: verify Recovery 1 on hardware before attempting isolated optimisations.
Do not redistribute daytona-vita-perspective-curves.vpk as a working fix;
its host math checks and replay hash did not demonstrate real GXM correctness.


## Vita perspective, widescreen cost and steering curves (2026-10-01)

Work remains on psvita-native-frontend; main and PSP branches untouched.
The Rendering/Enhancements design governs: presentation changes only, native
board timing unchanged, original aspect and linear steering remain defaults.

Road texture warp: previous affine subdivision used1/4/16/64 times as many
triangles depending on depth/span, then reduced precision under pool pressure.
Replaced it with homogeneous positions and the existing libvita2d WVP shader:
clip.w=depth, clip.z=0.5*depth, ordinary UVs. This gives perspective interpolation
with three vertices per source triangle. Painter order, stencil clips, tint,
solid/checker paths and fence ownership are preserved. Matrix changes are scoped
to textured batches and restored on all exits. No custom shader/compiler.
The exported libvita2d matrix is an internal ABI dependency documented with
upstream source revision and installed archive hash in platform/vita/THIRD_PARTY.md.
Recheck it when updating that library. Physical GPU output is not yet verified.

Widescreen had forced every tile layer through CPU composition and full uploads.
GXM now composes background tiles at the selected aspect, and foreground tiles
when centred. Sky margin colour is sampled through the same tile rectangles,
preserving split/window/scroll semantics. Active edge HUD alone uses shared CPU
grouping; unchanged source pixels reuse the grouped layer/upload even when
background scroll dirties drawing state. Pixel comparison, not a hash; extra
HUD source snapshot is about0.74MiB. Further distance still adds geometry.

Options adds persistent Linear/Soft/Extra Soft steering (linear, signed square,
cubic) after deadzone and before inversion. D-pad and full lock are unchanged.
Existing Test/Service mappings remain. Diagnostic logging staysOFF.

Validation:
- Host build and20CTest passes;2optional Lua tests skipped.
-5151 perspective sample comparisons and matrix projection/depth checks.
- Actual renderer submits3vertices for a depth100:1, wide triangle; no subdivision.
- All256stick positions across curves, monotonicity, endpoints/invert/D-pad tests.
- Final standalone renderer contracts pass, including eight sky comparisons
  (four split modes x normal/line scroll) against CPU top-left composition,
  unchanged-HUD upload reuse and matrix restoration.
- ASan/UBSan renderer contracts pass including sky comparison; final later test
  fixture enlarges the long-depth triangle, with the ordinary contracts rerun.
-6000frame16:9/edge-HUD/default-distance host replay:196665345i960,
  223429779TGP,3636sound-command bytes, hash9047513777edfaae, unchanged.
  42.27seconds on this host is not a Vita performance measurement.
- Public VitaSDK cross-build and VPK archive validation pass. Package:
  build/daytona-vita-perspective-curves.vpk
  SHA25666f209118967cb3c0beb55453d54a0f4dfb5b88c1bfda1b09bd683e2656aee87.
  Previous package retained at
  build/vita-enhancements/daytona_vita-before-perspective.vpk.

What not to re-propose: more affine subdivision to hide road warp costs both CPU
and vertex pool and remains approximate. Do not restore blanket CPU wide mode.
The former extreme-UV test rejected a finite polygon only because CPU u*q
overflowed; homogeneous submission keeps it finite, and its expectation was
updated. Host Ninja reported a truncated log and rebuilt fully on a subsequent
invocation; builds were sequential in each directory and completed successfully.
Next: confirm road lines, HUD/sky/clip transitions and frame pacing on real Vita.
No device FPS, crash-free-runtime or visually-fixed claim from these host tests.


## Vita GXM presentation options and cabinet binds (2026-10-01)

Switched to psvita-native-frontend and fast-forwarded to origin/bc02bb0 first.
No PSP files or commits were brought across; main and PSP branches untouched.
Upstream already contained desktop widescreen, per-item race-HUD gating and
draw-distance hooks, but the GXM frontend had none of their options connected.

Added persistent Aspect0..3, HUD edges and Draw Distance-2..2 settings, live
application on resume, scrolling16-row options and unchanged defaults.
Original GXM System24 fast path remains. Wide modes use shared CPU tile/HUD
composition, widened geometry clip planes and matching GXM projection/clip/HUD
offsets. Layer textures reserve896x384 for up to21:9;12MiB layer arena raises
totalGPU reservation30->32MiB. Wider/further options may cost FPS; no physical
performance claim.21:9 fits with vertical letterboxing.

Select+Triangle maps Test and Select+Square Service, consuming coin/view inputs.
Plain Select coin now triggers on release so staggered chords do not insert
coins. Menu latch suppresses a coin on resume; Start+Select remains frontend menu.
Controls regression covers both chords, staggered press and release.

Found existing generated C++ lacked hook_draw_list. Regenerated privately into
build/vita-enhancements-input/gen with seeds/daytona93_hooks.txt; onlychunk012
differs. Vita build links enhance.cpp and uses that generated tree. No generated
game code/assets are committed.

Validation complete: full host build,20CTest passes (2optional Lua skipped),
11build-helper tests and final GPU lifetime/layout/HUD ASan/UBSan pass.
Default and Furthest6000frame host16:9/HUD-edge replays complete:
default196665345i960 instructions, hash9047513777edfaae; furthest202533889,
hashc45f82273dcf08c6. Both223429779TGP instructions and3636sound-command bytes.
These are host replay results, not Vita speed or physical visual validation.

The first cross-build used old hookless generated code and was discarded.
An interrupted retry initially overlapped; both owned build trees' processes
were stopped, then a single build was resumed and completed. A host replay
link first missed the ymfm include path; corrected before the recorded runs.
The build helper now rejects GPU packages with a missing generated hook.

VPK: build/vita-enhancements/daytona_vita.vpk, archive integrity verified.
SHA256:95523616c6226f77293c8f71122f483aa1bbb9bc9973cc2e017483d9ba14da42.
ELF contains hook_draw_list and shared hud_polygon_offset; frontend objects
are newer than the final settings/control/source edits. Logging remains off.
Evidence: build/vita-enhancements-{package,final-tests,asan-final,
race-default,race-furthest}.log. Physical Vita testing remains required,
especially FPS at higher scenery levels and HUD appearance.
Changes are local on psvita-native-frontend; no push to main or PSP.

## Current state

**Force feedback: centring driver override, Linux (issue #16).** A PS2
Driving Force on Linux feels crashes but no centring on bends: the kernel's
Logitech driver offers, as far as known (hid-lg4ff from memory, not checked
here), constant force and autocentre but no spring, so the board's centring
spring is never played (autocentre is off: the game centres the wheel).
rt::DriveBoard::motor(x): the force the board's own closed loop drives for a
wheel at x (springs towards the centre over 8% beyond a 2%/6% dead zone,
uncentring away, the constant force as is, resistance nothing). Controls tab,
Linux only: "Centring driver override" (ffb_centring_override, off); on, the
constant force carries motor(wheel) from the game's own steer value
(io().inputs.steer) and the spring effect carries nothing. main passes it on
Linux only. Also: test_app_controls counted every joystick, so a real wheel
plugged in failed it (#16); it counts on top of what is there. Tests pass;
not tried on a wheel without a spring.

**Hide mouse cursor in game (issue #18).** Launcher, Game tab, next to
Fullscreen: "Hide mouse cursor in game" (hide_cursor, saved, off by default).
One rule in the main loop, every pass: hidden when the option is on, a game
exists and the launcher is closed; shown otherwise, so Esc (or a fault that
opens the launcher) shows it and Resume or Start hides it again, without
touching each place in_launcher changes. While hidden, ImGui's
NoMouseCursorChange is set: its SDL backend shows the cursor on every frame it
draws, and mobile draws its touch controls during play. Tried in the desktop
app (2026-10-07): hidden in play, shown on Esc, hidden again on Resume.

**Hold Test button (the test menu without F2).** rt::TestHold
(src/runtime/test_hold.h): holds the cabinet's Test switch (in0 0x04, active
low) for 173 frames (3 s), from game frame 240 at the earliest, then reports
it is done. Launcher, Game tab, next to Start: "Hold Test button"
(Config::hold_test, not saved; it clears itself when the hold ends); F2 and
any Test binding still work. For mobile, which has no F2 key. Measured first
(m2run, screen hash at frame 2,400): the game wants a fresh press from about
frame 200 after power-on; a press already down then is ignored even if held
to frame 287; a 3 s hold from frame 200, 240, 600 or 1,200 gives the same
screen as a 10-frame press at 1,200 (the test menu; the menu's
Test-selects-an-item does not fire). Revision A the same. test_test_hold. Tried in the desktop app
(2026-10-06): the test menu opens and the box clears.
Left for the Vita frontends' developer: GPU25 has no Test item; the plain
frontend's TEST SWITCH presses for one frame only, which can be missed; both
can use rt::TestHold. Dreamcast has no way into the test menu yet.

**Licence: BSD-3-Clause (LICENSE).** "Copyright (c) 2026, Ben Templeman and
contributors". The same licence as the MAME code transplanted into the
runtime; every linked component is compatible (BSD, zlib, MIT, public
domain). rules.md rule 9, THIRD_PARTY.md and the README say so; the open
decision is gone. GPL code (the MiSTer core, Supermodel) stays read-only.
Docs also brought up to date: the mobile notes folded into Current state,
PR #8's CI, branches now on main, #6 (Android and iOS, with touch, merged),
and the README's platform table: Android, iOS and PS Vita tested on devices
and working (2026-10-06), Dreamcast in Flycast.

**Force feedback: confirmed, enhancements documented (issue #9).** The
reporter confirmed on a Simagic Alpha Mini: no shaking, centring works, no
cut-outs. Documented in docs/issues.md, not started: a safeguard against
force feedback cutting out (SDL's DirectInput update reacquires a lost device
and retries, but its effects stay stopped: restart any effect
SDL_GetHapticEffectStatus reports stopped, about once a second; a theory from
SDL's code, seen in another recomp, not in Daytona), and settings asked for
after FFBPlugin: strength up to 200%, centring spring, resistance, a damper,
minimum force, with defaults that keep today's force feedback.

**Android and iOS (PR #14, platform/mobile).** The desktop runtime and its
SDL3/SDL_GPU renderer packaged for Android (arm64, SDLActivity, the game as
libmain.so) and iOS, from game code generated on a desktop (M2_GEN_ROOT). A
touch overlay on mobile only: analogue horizontal steering, gas and brake,
sequential gears, the four views, coin and start, and the launcher menu.
Fingers are tracked by device and finger ID (not ImGui's one pointer), short
taps last until a game frame, and focus loss, cancellation, the menu and
safe-area changes release them; touch merges with physical controls through
Controls, and the desktop gets no touch input. Android reads the ROM document
through SDL; iOS imports a private copy of each Files selection (a UIKit
document picker), the launcher fits the safe area and scrolls. UNSIGNED=1
platform/mobile/ios/build.sh makes build/ios/Daytona-unsigned.ipa for AltStore.
Runtime and shaders unchanged (the design's Platform layer & build). Checked:
Android assembleDebug and iOS device Release builds, plist lint, the unsigned
IPA; the ROM import and touch unit tests. Tested on devices (2026-10-06):
Android, iOS and PS Vita all play with no issues reported. Not recorded yet:
performance figures. (The iOS simulator cannot be used: it stops at
SDL_CreateGPUDevice, no SDL_GPU Metal there.) Found on an Android device: a
build that saved the picker's content:// URI in the config, reopened at the
next start, crashed on opening; a saved content:// is now cleared and the
archive asked for again (a Browse result is staged into app-private storage
first). If it still crashes at start, take a logcat before changing the import
again. Details: platform/mobile/HANDOFF.md.

**Windows setup: pin Clang discovery to VS 2022.** Both setup scripts now
select the documented VS 2022 toolchain when newer versions are installed
alongside it. CMake receives the matching generator and installation path;
mismatched cached configuration is cleared, preserving generated sources.

Checked: three setup regression tests pass. Revision A built with VS 2022
and Clang 19.1.5; 12 CTest tests passed, two optional Lua tests skipped.
CI now runs the setup tests and builds on branch pushes. Remote CI passed
(Build and Vita compile check; merged as PR #8).

**Frame pacing and fullscreen mode (issue #7).** src/app/pacing.h: Pacing
(Clock, Display, Vrr) chosen from the display's refresh and three settings,
all off (pace_smooth, pace_sync_display, pace_vrr), and Pacer, the per-pass
frame count. Clock is the old loop moved over unchanged (test: identical
frame counts over 20,000 random passes). Display: a frame every N refreshes
(Smooth: a multiple of 57.52 Hz within 1%; Sync: the first division of the
refresh at or under 61 Hz, if 56 or over: 60 on 60/120/180/240 Hz, nothing on
144/165), capped by the wall clock (vsync forced off) and restarted after a
stall. Vrr: one frame per pass, SDL_DelayPrecise to a 1/57.52 s deadline,
vsync kept (not IMMEDIATE as first planned: VRR with vsync and a frame cap
does not tear). Reference audio's stream ratio is scaled by game_hz/57.52 so
Sync to display does not overrun the 240 ms queue; native audio keeps its
device clock. The refresh is read every pass, so pacing follows the window
between displays (seen: 60 and 144 Hz). Fullscreen mode (fullscreen_mode,
"WxH@Hz"): the display's modes in the launcher, applied with
SDL_GetClosestFullscreenDisplayMode/SDL_SetWindowFullscreenMode. Measured
over 15 s in the game: default 57.9, Sync on 60 Hz 59.8, VRR 57.53 frames/s.
Design doc's open question on pacing answered. Not tried on a VRR display;
not listened to with Sync on.

**Legacy Logitech wheel support (issue #4).** The original Driving Force
(046d:c294) is listed but sends nothing through SDL 3.4.16's HIDAPI lg4ff
driver, which reads c294 reports only when they are exactly 27 bytes; the
reporter confirmed it works with SDL_JOYSTICK_HIDAPI_LG4FF=0. A setting, not
a change for everyone (SDL's driver serves the wheels it was written for):
Controls tab, "Legacy Logitech wheel support (Restart Required)",
legacy_logitech_wheels (on by default on Linux, whose kernel driver gives
Logitech wheels force feedback; off on macOS, where SDL's lg4ff haptics are
the only force feedback for them). On, main sets SDL_HINT_JOYSTICK_HIDAPI_LG4FF to
"0" before SDL_Init (SDL reads it when it finds devices; a change while
running is not guaranteed to hand the wheel back to evdev) and logs it.
Hidden on Windows, where SDL leaves lg4ff off already (hid.dll). The wheel's
GUID differs between the drivers, so it is bound again after switching.
Checked: a throwaway --profile with the setting on logs it, runs and saves it;
all CTest tests pass. Not tried with a Logitech wheel here.

**Audio tab: music and effects volumes (issue #10).** Launcher: a new Audio
tab with Volume, Mute, Music, Effects (music_volume, effects_volume: 1 and 1
by default) and the Native audio switch, moved from the Game tab. Reference
audio: at each MultiPCM key-on, SoundBoard::voice_channel reads the owning
channel from the driver's voice pools in 68000 RAM (0xf01500 and 0xf01618, 28
records of 10 bytes; byte 0 nonzero in use, bit 3 the chip; byte 1 the slot
code; byte 3 the channel; byte 6 0xff on the engine layers' reserved records,
skipped: they carry chip 0's bit with chip 1's fixed slot codes, which gave
1,597 false double matches in a race before they were skipped). Music is
channels 0-9 and 15 (NativeSoundSequencer::music_channel, the driver's "stop
music" set); a key-on with no record (the engine's fixed slots 18-27 on chip
1) is an effect. MultiPcm sums music and effect slots apart and scales them
before its 16-bit clamp; at 1 and 1 the sums are added as before. Native
audio: VoiceEvent::music, NativeSampleMixer::set_volumes per voice, set from
the audio callback through atomics.
Checked: class against the native sequencer's channel, note for note
(scratch program on the oracle's pairing): daytona93 race 4,899/4,899, attract
2,274/2,274; daytona race 4,892/4,892, attract 2,095/2,095; one voice record
per key-on. race_basic reference WAV byte-identical before and after (20 MB);
screen hash ad67233983ea8808 and instruction counts unchanged. Music alone +
effects alone = the full mix except 0.097% of samples, where a chip clips in
the full mix. Race RMS: effects 0.195, music 0.059 (10 dB). New tests:
multipcm_volumes, native_sample_mixer's volumes case; all CTest tests pass.
Not listened to on this machine.

**Force feedback: Daytona's drive board commands (issue #9).** The command
meanings taken from Supermodel's later drive boards were wrong for Daytona:
the game's centring spring (0x39-0x3C, on all race) was played as a 60 ms
sine, so wheels shook constantly with no centre pull, and pads buzzed. Checked
against the drive board's own program, EPR-16488A: disassembled in MAME
0.289, then run with Lua taps on its I/O (commands fed on port $27, wheel
positions on its ADC at $80, motor power and direction read from $46).
rt::DriveBoard now follows it: motor on/off (0x0-), one effect at a time with
strength in the low 3 bits (0x2- resistance, 0x30-37 centring spring, 0x38-3F
the same with a dead zone, 0x4- uncentring, 0x5-/0x6- constant force), no
force for 0x1- and the x8-xF halves, queries (0x8- up) changing nothing; no
vibration (the board has none). app::ForceFeedback: spring (negative for
uncentring), friction and constant force; pads rumble only for the constant
force and uncentring; a wheel whose haptics fail to open gets nothing instead
of SDL's DirectInput rumble (a sine through the wheel) and is retried every
3 s; failed effect updates are retried; pausing keeps the board's state
(ffb.reset() on a new game). Launcher: "Log force feedback" (ffb_log, off).
Checked: test_app_controls and all CTest tests pass. Not checked on a real
wheel; which of 0x5-/0x6- is left on a cabinet is not known (Invert force).
Details and the command table: docs/issues.md.

**Open issues reviewed (docs/issues.md).** Each open GitHub issue checked
against the code: #4 (Driving Force on Linux: SDL 3.4.16's lg4ff driver drops
input reports that are not 27 bytes; ask for a run with
SDL_JOYSTICK_HIDAPI_LG4FF=0), #7 (frame pacing; four display settings
planned, all off, so the default stays native speed, 57.52 frames/s), #9
(force feedback; the drive board's commands checked against its program),
#10 (reference audio's mix matches MAME; native audio's master gain 1.95 and
missing sample envelopes) and #6.

**Dreamcast port (in progress; on main since PR #11).** Now about 32 frames/s in
Flycast (reported 2026-10-06; the work behind it is not on main yet, so the
measurements below stop at the older figures). platform/dreamcast: a
KallistiOS build run by platform/dreamcast/build_dreamcast.py after the
desktop build, from the `daytona` (Revision A) set only, like platform/vita.
Runtime changes are behind M2_DC_MEMORY (ROM read through a page cache the
frontend supplies, texture and frame buffer RAM supplied, a two-level page
table, no GPU-layer copies, lazy rasterizer buffers, reused Lockstep
callback slots, geo_test's cursor moved on without its loop), M2_DC_SPIN_SKIP
(the game's frame-wait loop at 0x1394 skipped by whole passes to the next
lockstep event: 79% of the i960's instructions in a race) and M2_DC_SPEED
(the same output, faster: tile rows with nothing to draw skipped, geometrizer
object data parsed only for frames that are shown, frame skip 3 allowed, tile
layers composed in the PVR's 16-bit formats);
the desktop defines none of them. Checked after the changes: build-daytona m2run
race_basic, screen hash 9427a612c5cb7511, 909,312,001 i960 and 195,261,176
TGP instructions, as before. In Flycast a whole recorded race (race_basic) runs,
drawn by the PVR with textures and both tile layers, in lockstep with the
desktop at all 100 checkpoints; 25 frames/s with every 2nd frame drawn, the default, with sound (175.9 s for the race; 29 frames/s, 207 s, with every 4th; 1,414 s at the start); with the controller it runs until Flycast is closed. The geometrizer's transforms by the SH-4's ftrv (M2_DC_NATIVE_GEO, picture only; --no-native-geo turns it off): race 240.8 -> 237.0 s in Flycast, lockstep exact. Tile layers composed again only on the lines that changed (M2_DC_SPEED, Video::compose16; dcmemcheck's layer hashes identical): 237.0 -> 228.5 s. The race's scenery layer (pixmap layer 2) drawn by the PVR from a video RAM texture at its scroll instead of composed every frame (M2_DC_SPEED; dcmemcheck rebuilds and compares it): 228.5 -> 218.2 s. The geometrizer's ROM reads in blocks (GeoPtr::read, M2_DC_SPEED; dcmemcheck identical): 218.2 -> 212.8 s. Two remembered ROM pages per region and an FPU finiteness test in the renderer: 212.8 -> 205.3 s. The i960's and the TGP's instruction counts kept in registers by fast_gen.py (dcfastcheck identical every frame): 205.3 -> 195.1 s. Renderer radix sort and colour cache: 195.1 -> 193.4 s. No library memcpy/memset for kept polygons and register frames (M2_DC_SPEED): 193.4 -> 187.5 s. Tile block compare without memcmp: 187.5 -> 186.5 s. Culled polygons skip their texture reads and lighting (M2_DC_SPEED): 186.5 -> 180.1 s. Two-level entry switch and binary chunk search in the i960 code's dispatch (fast_gen.py): 180.1 -> 176.9 s. The runtime reads count + Lockstep::pending during a call, so the generated code stores its count once instead of adding it (M2_DC_SPEED): 176.6 -> 175.9 s, 0.21 MB of RAM back. The play build prints no TRACE lines and reports every 1,800 frames (the serial port is slow on a console). Found for the desktop too, not changed
there: Lockstep::calls_ never shrinks (a callback every 1,024 instructions;
estimated 270 KB a second on the desktop). KOS 2.2.1 in extern/kos-dc
(git-ignored): DreamSDK R4's installed KOS master stops every C++ program
using libstdc++'s exceptions at startup. Details, measurements and failures:
platform/dreamcast/HANDOFF.md.

**Vita build: the ROM set.** The 1994 set's M2_ROMSET broke the Vita compile
check (its CMake builds the runtime itself, without the define);
platform/vita/CMakeLists.txt defines M2_ROMSET="daytona93", and
rom_import.h falls back to daytona93 when a build does not say. Checked on
the link-play branch's CI: Vita compile check passes (with link play's
comm_board added to the Vita runtime there).

**Link play (on main).** Revision A's communication board
(837-10537), from MAME's m2comm simulation: src/runtime/comm_board.{h,cpp}
(the protocol: shared RAM set-up at cn_w, the master's 0xff/0xfe numbering
tokens round the ring, every frame each cabinet's 0xe00-byte block from
shared 0x2000 to the next, landing at 0x21c0, and the master's 0xfc vsync;
frame sync optional, off by default as in MAME). M2Board::set_link attaches
it to a LinkTransport; with none (the default, and daytona93) the comm
registers stay plain, so nothing changes. src/app/link_socket.cpp: TCP, this
cabinet listens, connects to the next (retried until it answers), non-
blocking reads, whole-frame sends with a 2 s limit, POSIX or Winsock. App:
launcher Game tab "Link play" (on/off, port, next host:port, frame sync,
status line); daytona --profile NAME for a second cabinet on one computer;
m2run --link-listen/--link-next/--link-sync and --save-nvram. Master and
slave are the game's own settings (test mode > GAME SYSTEM: LINK ID, CAR
NUMBER; factory: MASTER, car 1, twin). Checked: tests/test_comm_board.cpp
(two boards in memory: numbering, data both ways, frames in pieces, loss);
two headless m2run cabinets on localhost (master: factory EEPROM; slave: one
set to LINK ID SLAVE, CAR NUMBER 2 through test mode by script): the
linked attract (通信システム 2人まで対戦できます), both through the linked
course and transmission select, a race with POSITION /2 on both, red car
(car 1) and blue car (car 2); "cabinet 1 of 2" / "2 of 2"; the link lost
when the other cabinet quits. Not checked: two computers, Wi-Fi, frame
sync, more than three cabinets.
First two-computer try (Mac 192.168.1.52 slave, port 15112, next
192.168.1.2:15113; the other the master): NETWORK CHECKING, because the
other computer was not listening on 15113 (connection refused: its port was
still the default 15112). The launcher's waiting status now says which half
of the ring is missing (next cabinet reached or not; a cabinet connected to
this port or not), and the help says every computer can use the same port.
Then linked on two computers once the cabinets' settings matched (the game
cancels the link, "CANCELLED", when they differ: one was DELUXE/USA, the
other TWIN/JPN). Native audio then stopped the game: linked play's music
turns on the MultiPCM LFO (MIDI controller 0x01, value 1, channels 0 and 5;
found with the new m2run --native-audio-check on a headless linked pair,
frames ~1,510), which the native mixer does not have, and the app treated
any unsupported effect as a fault. Now it plays without the vibrato and
says so once in the log; invalid data and callback failures still stop it.
Open: the LFO in the native mixer (the driver's controller 0x01 handler to
MultiPCM registers 6/7, MAME's multipcm LFO).
Tested by the user: three computers linked (Windows, macOS and Linux), all
played well.

**The seed_scan seeds checked against MAME (daytona93).** A local MAME
build (scripts/build_mame.sh) and scripts/m2_check.sh on every scenario: the
11 in scripts/inputs and two new ones, attract_long (9,000 frames of
attract) and race_to_end (race_basic played on to 20,000 frames: out of
time, game over and the screens after). All match MAME in every check:
native i960 code (every device event and interrupt), the native
geometrizer, the CPU 3D layer, the composed screen, the recompiled TGP and
the sound 68000. race_to_end alone: 2.16 billion i960 instructions, 19,997
frames, 20.2 million polygons, 597 million TGP and 261 million 68000
instructions identical. Coverage (MAME's indirect-branch log against the
seed list): the 11 old scenarios reach all 334 harvest seeds and none of the
248 seed_scan seeds (they are the harvest's own scenarios); attract_long and
race_to_end reach 81 of them (10 of 19 game modes, 13 of 29 task states, 3 of
85 jump-table entries, 5 of 16 lda tables, 48 of 92 ROM-record handlers, 2 of
7 data-ROM pointers), now checked. The other 167 need states no scenario
reaches yet (other endings, name entry, link play). Found on the way:
- An Apple silicon MAME differed from ours in the geometrizer's last bit
  (attract frame 173): clang fuses a*b+c into FMA on arm64 by default;
  build_mame.sh now builds it with -ffp-contract=off, like our code.
- build_mame.sh picks a Python whose XML parser loads (Homebrew's Python
  3.14 here had a pyexpat built against a newer libexpat: "No parsers
  found"); setup.sh --build-mame on macOS installs sdl3 and pkgconf (MAME's
  macOS front end is SDL 3, found through pkg-config), not sdl2.
- The first m2_check run of race_to_end stopped at 6,897 frames (cause not
  found: MAME's output goes to /dev/null there); run again by hand, MAME
  recorded all 19,997 frames and every check matched.

**Setup builds the 1994 set too.** scripts/setup.py builds every set it
finds in roms/: daytona93 into build/ (as before), daytona (Revision A) into
build-daytona/ (configured as the main build, with -DM2_ROMSET), and prints
how to start each. The rejection and no-ROM messages name both sets.
getting-started.md and the README describe both, including Revision A's
first run (a single cabinet set in test mode). Checked: setup.py from a
configured tree with both sets in roms/: both games built, tests pass.

**Widescreen: scene or 2D screen, by window and coverage.** The margins'
fill (stretched or sky behind a 3D scene, each row's edge colours on a 2D
screen) was chosen by "the 3D covers half the screen". Attract and race
camera shots that look at a lot of sky cover 36-49% (measured: daytona93
attract frames 720-780, 6720-6860, 7020-7140, 8320-8460; Revision A the
same kind), so those frames smeared their sky's edge colours sideways and
then snapped to stretched when the camera moved. Video::scene(): one window
and at least 15% coverage. Measured over attract and race_basic on both
sets: daytona93's car and circuit select draw their 3D in 2-3 windows,
Revision A's select screens have no 3D, titles none; every one-window frame
with 15-49% is a scene. Checked: daytona93 attract frame 760 now stretched,
circuit select still edge colours (16:10, stretch on).

**Wheels, pedals and force feedback.** Controls gained a third binding
column, Wheel / joystick: every SDL joystick is opened (app::Devices), and
an action binds to an axis, button or hat direction of a device by GUID, so a
wheel, pedals and a shifter can be separate devices. An axis is calibrated
when bound: the capture records where it rested and how far it was moved
before being let go (pedals resting at either end or short of full range; a
900-degree wheel's chosen lock). Saved as `<action>.joy=` lines, with
`joy_deadzone`. Force feedback: the game writes the drive board's command to
I/O board dual-port RAM byte 0x11 (found by logging its writes: 44 in a
race, every type), which IoBoard queues; rt::DriveBoard decodes them (the
meanings below were wrong for Daytona and are replaced: see "Force feedback:
Daytona's drive board commands" above. Then: the
command set of Sega's later drive boards, as Supermodel documents it:
0x1- centring, 0x2- friction, 0x3- vibration, 0x5-/0x6- pull right/left, 0xc-
reset; 0x0-/0x4- sequences and 0x7- not modelled) and app::ForceFeedback
plays them on the steering device: SDL haptics (spring, friction, sine,
constant force on the steering axis; only changed levels sent) or rumble.
Launcher: Force feedback strength (70% default, Off) and Invert force; m2run
prints the commands by type. Checked: tests/test_app_controls.cpp with SDL
virtual joysticks (a wheel and pedals resting at +32767; config round trip;
ADC values; a wheel paddle; unplugging; drive command decoding; rumble and
its scaling and stop). Not checked: a real wheel or force feedback (none here; user
testing): the launcher marks the wheel column, its dead zone and force
feedback Experimental; the direction of the pull may need Invert force.

**The 1994 set (daytona, Revision A) builds and runs, for comparison.**
CMake `M2_ROMSET` (daytona93, the default, or daytona), one set per build
directory: `scripts/recompile.py --set daytona --build-dir build-daytona`
with the set at roms/daytona.7z. The importer has both sets' MAME tables
(Revision A: other program, sound program, two main data ROMs mirrored from
0x800000, two polygon and two texture ROMs; the TGP program is the same
2,024 words, at main_data 0x800020). Saves are per set (pref folder
daytona-recomp/<set>). Seeds: seeds/daytona.txt, the daytona93 seeds carried
over by code (new scripts/seed_map.py: 469 of 582, 175 of them by
alignment where the revision inserted code), then seed_scan.py --set
daytona (239 more; 38,001 instructions reachable). Revision A does some
float maths on the i960's FPU instead of the TGP: m2recomp gained addr, subr
and modi (MAME's semantics; register forms only; daytona93's generated code
unchanged). Hooks: seeds/daytona_hooks.txt, draw distance at 0x17508
(daytona93's draw-list routine moved by 0x490: the same masked instructions,
RAM and boot-time budget); checked: -2 and +2 change the frames as on
daytona93. Its default settings are a linked twin cabinet, which waits on the
link at the settings screen: set a single cabinet in test mode (F2) once;
m2run and m2gpushot `--nvram DIR` load the app's saved EEPROM and backup RAM
(tools/common/nvram.h). Checked with that: 12,000 attract frames, and every
input script (races on all three courses, steering, test mode screens)
runs with no missing code; race_basic plays through circuit select,
transmission select, the rolling start and the race, with drive board
commands. Native audio works on Revision A too: its sound program has the
native sequencer's tables from 0x505a on 0x48 bytes later, their pointers
with them (sequence banks and engine sound tables unmoved); the sequencer
detects the layout by those tables' first pointers. scripts/
test_native_sound_oracle.sh takes the set from the ROM folder's name and
M2_NVRAM=DIR: Revision A race, 6,000 frames: 4,892 of 4,893 notes the same
as the reference audio (the extra one at the run's last instant), the same
two pitch differences as daytona93, RMS ratio 1.02, no invalid data.
Why: the arcade's wheels sit inside the wheel arches with a gap; ours (and
MAME's) poke out over the wings. The car code and data are the same in both
revisions (wheel table at 0x234af4 / 0x230d54: ±0.525, 0.32, 1.4125/-1.4;
body and wheel models on shared polygon ROMs), and Revision A here frames
the arcade footage's bridge shot (frames 8680-8780) almost exactly and still
shows the tyres over the wings: not a revision difference. In attract the
car is moved by the course-following routine (0xca40), so body roll is 0
and pitch small; the full physics (0xf31c) does not run. Open: what on the
board differs (the geometrizer port, the TGP).

**Super sampling (hardware renderer).** Launcher > Game > Super sampling:
Off, 2x, 3x, 4x (`supersampling=` in launcher.ini; m2gpushot
`--scale N`). GpuRenderer::render draws into a target `scale` times the
frame: vertices stay in original pixels (the viewport scales them), clip
rectangles and the depth buffer scale, the tile shaders map target pixels
back to original ones (tiledata[2]; tile pixels repeated), and textures
take a finer mip level, log2(scale) levels (texlod + 128 log2(scale); the
rasterizer picks levels from z, not screen size). The checker pattern is
per target pixel. main draws it into its own texture with mip levels and
shows the level nearest the window's size, so a frame bigger than the
window is averaged down (supersampling). 1x: byte-identical to before (104
race frames, 16:9). race_basic 16:9 headless (Metal): 455 / 389 / 295 /
265 frames/s at 1x / 2x / 3x / 4x. Checked: a 3x race frame (no cracks,
HUD in place) and the app at 3x (700 frames).

**Tilemaps, step 2: drawn on the GPU (hardware renderer).** m2.hlsl
ps_tiles_back / ps_tiles_front compose the System 24 layers per pixel with
Video::draw's rules (window masks, per-line scroll, the split modes, the
back pass's opaque 3 and 2), then the widescreen margins as fill_margins
(edge colours, the sky's colour, or stretched). Inputs: the decoded pixmaps
(a u16 per pixel, pen and category; uploaded by the span of tile rows
changed since the last upload, by the tile generations; never with
SDL's cycle flag, which would drop the rows not sent), and a per-frame
snapshot Video takes at screen_update of tile RAM 0x4000-0x6fff and the
4,096 pens. Video's external-3D `cpu_layers` is now `desktop`: the CPU only
estimates the 3D coverage (margin fill) and, with the HUD at the edges in a
race, still draws the front layers and moves the HUD blobs (uploaded as a
texture then). Checked against the previous build (CPU-drawn layers):
byte-identical frames, 0 pixels differ, over race_basic at 4:3, 16:9,
16:9 stretched and 21:9 with the HUD at the edges, the advanced and expert
courses, test mode and test drive (622 frames; the races use split modes
and per-line scroll). race_basic 4:3: 400 -> 526 frames/s; game 1.82 ->
1.03 ms, renderer CPU 0.53 -> 0.33 ms, GPU 0.16 -> 0.54 ms. Tested by the
user on macOS (Metal), Windows and Linux.

**Tilemaps, step 1: decode only changed tiles (both renderers).** Measured
first (m2gpushot --bench now reads Video's own timers): of the hardware
frame's 2.2 ms game time, the CPU tilemaps took 1.27 ms: decoding the four
512x512 layers 0.55 ms (all 16,384 tiles, every frame), drawing them with the
scroll/split/mask rules 0.63 ms, composing 0.09 ms. Video::decode_layers
(desktop; the Vita path keeps its own cache) re-decodes only tiles whose
value or character changed, comparing character RAM (256-byte pages, then
32-byte characters) only on frames the game wrote it. Same pixmaps: race,
time attack, test mode and attract screen hashes unchanged. Decoding 0.55 ->
0.02 ms; hardware 343 -> 409 frames/s, software race 189 -> 204. Next: the
layers drawn on the GPU (0.65 ms drawing + 0.09 composing).

**Vulkan application name (MangoHud showed "SDL").** SDL's Vulkan backend
hard-codes VkApplicationInfo: no application name, engine "SDLGPU", which
overlays such as MangoHud show instead of the API; SDL has no property to
change it. `patches/sdl3/0001-vulkan-application-name.patch` (applied by
setup.py's new apply_patches, shared with MAME's patches; already-applied
patches are skipped; fetch forces the checkout if a patched file would block
a new pin) reports SDL_SetAppMetadata's name ("Daytona USA", set by main)
and no engine name. Checked: setup re-applies it after a revert and skips it
when present; macOS builds and runs. The MangoHud result itself is untested
here (no MangoHud on macOS).

**Windows: the game's messages.** daytona is a WIN32 (GUI) program, so on
Windows its output went nowhere and a command window returned at once; a
tester could not see which renderer ran. It now attaches to the parent
console when there is one, else writes daytona.log in the pref folder, and
prints `daytona: renderer hardware (GPU)` / `software (CPU)` whenever the
active renderer changes. Checked on macOS (both lines); the Windows branch is
compiled by CI only.

**Hardware renderer speed (`m2gpushot --bench`).** race_basic, 6,000 frames,
headless on this Mac (Metal): software 189 frames/s (5.3 ms a frame) at 4:3
and 188 at 16:9; hardware 324 (3.1 ms) and 347. Hardware frame at 4:3: game
2.20 ms (logic, geometrizer, CPU tilemap layers), renderer on the CPU 0.72 ms
(vertices, uploads, a 4 MB texture RAM compare, the colour table), waiting
for the GPU 0.17 ms. From the draw-mode figures (every third frame drawn:
442 frames/s), logic is about 0.75 ms, the CPU tilemap layers about 1.45 ms
and the CPU 3D rasterizer about 3 ms a frame. Next costs, in order: tilemaps
on the GPU; texture RAM tracked by writes instead of compared.
Done: M2Board::tex_write counts writes (VideoMem::tex_generation) and the
renderer uploads texture RAM only when the count moves; the 4 MB compare and
shadow copy are gone. Renderer CPU 0.72 -> 0.54 ms, 324 -> 346 frames/s at
4:3; GPU frames byte-identical to before (8 of 8 sampled).

**Hardware renderer, widescreen and a mip-level fix.** Hardware mode keeps
widescreen: Video's external-3D mode with CPU layers no longer drops the
margin (only the Vita path does); both layers are width() wide, the
backdrop's margins filled as in software mode with the 3D coverage taken
from Raster::coverage_estimate (the polygons on an 8x8-pixel grid; no CPU 3D
layer exists here) and the front layer's HUD moved to the edges; the GPU
projects with the margin, widens full-width windows into it, and moves the
condition panel's quads by Video::gpu_hud_shift (same box and z as the
software path). Fix found while comparing: the rasterizer's max mip level is
30 - countl_zero(min(w, h)) = log2(min) - 1; stage 2 used log2(min), so a
fading circuit-select map (texlod -321, mml 1132) took level 7 not 6 and
came out coloured instead of grey. Measured after (race_basic, Metal,
m2gpushot vs m2run, every 650 frames to 5200): 89.7-100% identical,
94.4-100% within 8 levels; the rest are rounding on high-contrast textures
(road lines, rock), where a one-step texel coordinate difference flips the
blend. 16:9 with HUD at the edges: race frames 94.8-98.3% identical.

**Hardware renderer, stage 2 (textures).** ps_poly is a port of the
rasterizer's draw_tex_span and fetch_bilinear_texel in integer arithmetic:
the 4-bit sheets with their 2048x1024-as-1024x2048 mapping, bilinear 8-bit
blending (LERP), wrap/mirror/edge rules, mip levels by fast_log2 (the same
128-entry table) and texlod, the microtexture, the translucency flag and
test, the luma RAM and the colour translation. Data: three read-only storage
buffers (texture RAM, both sheets, uploaded only when it changes; luma RAM;
colour translation with the rasterizer's gamma applied on the CPU); per
polygon texture state as flat integers decoded as render_one does; 1/z, u/z,
v/z interpolated noperspective. Measured (race_basic, Metal, m2gpushot vs
m2run): frame 1500 99.9% of pixels identical; race frames 3000 and 4500
95.5% and 95.1% identical, 98.4% and 98.8% within 8 levels. The differences
are scattered over textured surfaces (far road, rock face), not edges:
float differences flipping mip-level and texel thresholds. The rasterizer
accumulates 1/z, u/z, v/z per pixel along each span; the GPU evaluates each
pixel's directly, and Metal compiles with fast math. Exactness is stage 4.
build_shaders.py pulls the x86-64 Ubuntu image explicitly (a cached arm64
one failed with "exec format error").

**Hardware renderer, stage 1 (geometry).** Launcher > Game > Renderer:
Software (exact; default) or Hardware (Experimental). `src/app/gpu/`:
`m2.hlsl` (one source) -> `scripts/build_shaders.py` (DXC v1.9.2609 to
SPIR-V and DXIL, SPIRV-Cross to MSL; Docker when the tools are not
installed) -> `shaders_gen.h` (committed; builds need no shader tools).
GpuRenderer draws the 3D in the rasterizer's order (window, then z, newest
first), projected as model2_3d_project, each polygon a fan from vertex 0,
clipped to its window by scissor; a depth buffer with depth = draw order
and LESS reproduces "first polygon to fill a pixel wins" (the rasterizer's
fill buffer). Colour: the solid renderer's palette/luma/gamma; textures are
stage 2 (the Vita GPU path is a reference only: a tester saw small road
geometry/orientation errors there). Video's external-3D mode gained
`cpu_layers` (the CPU still draws the tilemap layers for it; the Vita path
does not). No widescreen in hardware mode yet. `m2gpushot` renders frames
offscreen through it for comparison with m2run's (tools/common/
input_script.h shared). Checked on Metal: the game runs (710 frames in
15 s), and a race frame's geometry, HUD and backdrop line up with the
software renderer's. Not yet run on Vulkan or Direct3D 12.

**Draw mode (frame skip).** Measured first: Daytona runs the board in 60 Hz
mode and the geometrizer starts a new frame every vblank (3,000 of 3,000
race frames drew a new 3D picture), i.e. double buffered. Launcher > Game >
Draw mode: Double buffered (every frame, the default), Single buffered
(every 2nd), Every third frame (every 3rd); `m2run --frame-skip 0|1|2`.
M2Board::vblank_end skips screen_update on the frames between (3D raster,
tilemaps, composition), keeping the last picture; the geometrizer still
parses every frame (the game reads its polygon count). Measured race_basic:
identical i960 (196,665,345), TGP (223,429,779) instruction, interrupt
(12,050) and sound byte (3,636) counts in all three; 190, 332, 442 frames/s
headless on this Mac. Default hash unchanged.

**Skip launcher.** Launcher > Game > "Skip launcher" (saved): start-up goes
straight into the game, as `--autostart` does, when the ROM set checks out;
otherwise the launcher shows with the reason. Esc still opens it. Checked:
with it set, `daytona` started the game at once (377 frames in 8 s).

**"Stretch tile background" stretches, it does not extend.** In a race, with
the option on, the backdrop as drawn for the 496 columns is scaled across
the whole width (linear blend per row); the tester wanted no repeat at all.
Video::draw_ext (drawing the tiles past the screen edge) is removed. Earlier:

**Widescreen sky: plain by default, "Stretch tile background" to extend.**
A tester still saw a seam in the margins with the tile backdrop drawn out.
Measured: the race sky is tilemap layer 2, one layer in normal scroll mode
(not a split pair: the split-mode alternation added to draw_ext changed 0
pixels there), and its hscroll sweeps the whole 0..511 range over a lap
(152 values), so the original 4:3 screen passes the picture's join too;
only ~79% of rows match across it. So the margins default to the sky's
plain colour behind 3D, and launcher > Enhancements > "Stretch tile
background (Experimental)" (`m2run --stretch-backdrop`) draws the tiles out.
2D screens keep each row's edge colours either way. Split pairs in draw_ext
now alternate A/B every 512 columns (a 1024-pixel panorama), as the
hardware's layout implies; not exercised by Daytona's race sky.

**Launcher labels.** Options that need it say so: "Graphics API (Restart
Required)"; "Native audio (Experimental, Reset Required)" (it applies when a
game starts or is reset, not on an app restart); "HUD at the screen edges
(Experimental)". Everything else applies straight away.

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
a group move with it, but only while the race HUD is on screen: a tester
saw the car's door come off in an attract close-up, because the car's own
near polygons (and the ranking screen's 2D markers) sat in the HUD's
right-hand area and were moved. Now nothing moves unless the frame has the
condition panel's box (one checker-shaded overlay polygon, texheader
0x8000, sort z <= 0x0fff, ~77x82 at x 385..462, y 67..149;
Raster::race_hud_visible). Measured after: 240 attract frames at 16:9
identical with the option on and off; the race HUD still moves, and the
map stays at the edge through the rolling start. A tester still saw 3D
moved in play: the rule was any polygon at sort z <= 0x0fff inside the right
group. Now only the condition panel's own quads move: polygons at exactly
the box's z inside the box's outline (Raster::find_race_hud records both).
Measured: race at 16:10, option on vs off, 0 of 120 frames differ outside
the HUD areas.
Side margins: in a 3D scene (the 3D layer covers >= 50% of the screen;
measured races 69-100%, select screens ~23%) the back layers are drawn
margin to margin by Video::draw_ext, draw()'s rules pixel by pixel for any
screen column: scroll, per-line scroll, the split modes that put layers
L and L^1 side by side, priority, window masks. Checked: its visible columns
equal draw()'s on every frame of a race (M2_CHECK_DRAW_EXT=1 prints any
difference; none). Earlier tries, all wrong in play: one plain sky colour;
each row's edge carried out (smeared the clouds); copying columns mod 512
(a tester saw the backdrop duplicated with a seam: a column off the screen
can belong to the other layer of a split pair); a "joins up across the
wrap" test to tell sky from menus (failed on the race sky, median 79% of
rows). On 2D screens (car, circuit select) each row carries its own edge
colours: their art covers only the 496 columns, and drawing further shows
leftover tiles as stripes. Before that the margins were the sky's plain colour (the back
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

1. Scenarios that reach the other 167 seed_scan seeds (other endings, name
   entry, link play), checked with scripts/m2_check.sh like attract_long and
   race_to_end.
2. Hardware renderer: exact pixels against the CPU reference (stage 4).

## Open decisions

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
