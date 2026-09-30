#!/usr/bin/env bash
# Real-ROM dense/paged audit using the same native generated code. Explicitly
# recompiles layout-sensitive runtime sources with the PSP low-memory switch
# and normal CPU rasterizer (no Vita render optimizations). Both boards use the
# sparse page table: this compares dense ROM storage against file-backed ROMs,
# not sparse versus flat page-table implementations. It does not change the
# normal desktop build or require the PSP toolchain.
#
# Full race geometry/memory/command parity:
#   bash scripts/test_psp_memory.sh build 6000
# A visible 240-frame CPU framebuffer window after attract-mode boot:
#   bash scripts/test_psp_memory.sh build 1680 build/rom_cache/daytona93 attract 1440 240
#
# Compare an all-CPU attract final hash with the regular desktop runner:
#   build/m2run build/rom_cache/daytona93 1680
#   bash scripts/test_psp_memory.sh build 1680 build/rom_cache/daytona93 attract 0 1680
#
# SKIP_BUILD=1 reuses current generated host objects and libraries. Outputs and
# imported data remain in the ignored build tree; no ROM assets are committed.
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$repo_root/build"}
frames=${2:-6000}
rom_dir=${3:-"$build_dir/rom_cache/daytona93"}
mode=${4:-race}
render_start=${5:-0}
render_frames=${6:-0}
if [[ $# -gt 6 || ! $frames =~ ^[0-9]+$ || ! $render_start =~ ^[0-9]+$ ||
      ! $render_frames =~ ^[0-9]+$ || ( $mode != race && $mode != attract ) ]]; then
    printf 'Usage: %s [BUILD_DIR] [FRAMES] [ROM_DIR] [race|attract] [RENDER_START] [RENDER_FRAMES]\n' "$0" >&2
    exit 2
fi
if [[ ${SKIP_BUILD:-0} != 1 ]]; then
    cmake --build "$build_dir" --target m2run -j "${BUILD_JOBS:-2}"
fi
objects=("$build_dir"/CMakeFiles/m2run.dir/gen/daytona93/*.o
         "$build_dir"/CMakeFiles/m2run.dir/gen/daytona93_tgp/*.o
         "$build_dir"/CMakeFiles/m2run.dir/gen/daytona93_snd/*.o)
for object in "${objects[@]}"; do
    if [[ ! -f "$object" ]]; then
        printf 'Missing generated host object: %s\n' "$object" >&2
        exit 2
    fi
done
libraries=("$build_dir/libruntime.a")
if [[ -f "$build_dir/liblzma7z.a" ]]; then libraries+=("$build_dir/liblzma7z.a"); fi
libraries+=("$build_dir/libtrace.a" "$build_dir/libi960.a"
            "$build_dir/libsoftfloat.a" "$build_dir/libymfm.a")
sources=("$repo_root/tests/test_psp_memory_rom.cpp")
for source in paged_rom m2_board m2_tgp_board geo game_loop video raster; do
    sources+=("$repo_root/src/runtime/$source.cpp")
done
# These objects take precedence over the matching dense runtime archive
# members. Generated code touches only the stable runtime bus interfaces.
"${CXX:-c++}" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
    -DM2_LOW_MEMORY=1 \
    -DSOFTFLOAT_FAST_INT64 -DLITTLEENDIAN=1 -DTHREAD_LOCAL=__thread \
    -I"$repo_root/src" -I"$repo_root/extern/ymfm/src" \
    "${sources[@]}" "${objects[@]}" "${libraries[@]}" \
    -o "$build_dir/test_psp_memory_rom"
"$build_dir/test_psp_memory_rom" "$rom_dir" "$frames" "$mode" "$render_start" "$render_frames"
