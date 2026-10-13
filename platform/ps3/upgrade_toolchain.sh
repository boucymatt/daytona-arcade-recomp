#!/usr/bin/env bash
# Reproduce the isolated compiler build; never install over the working SDK.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
stage="$repo/build/ps3-sdk"
source_dir="$repo/build/ps3-toolchain-source"
pin=e13aa539c07abe86cdbcd095dc1340492dbca213
mkdir -p "$stage" "$source_dir"
if [[ ! -f "$stage/ppu/bin/ppu-gcc" ]]; then
    cp -a /usr/local/ps3dev/. "$stage/"
fi
if [[ ! -f "$source_dir/scripts/002-gcc-newlib-PPU.sh" ]]; then
    curl --fail --location "https://codeload.github.com/ps3dev/ps3toolchain/tar.gz/$pin" \
        --output "$source_dir/upstream.tar.gz"
    tar -xzf "$source_dir/upstream.tar.gz" -C "$source_dir" --strip-components=1
fi
export PS3DEV="$stage" PSL1GHT="$stage"
export PATH="$stage/ppu/bin:$stage/bin:$PATH"
export CC=clang CXX=clang++ NO_SAVANNAH=1
export CFLAGS_FOR_BUILD='-Wno-error=incompatible-pointer-types -Wno-error=int-conversion'
# Old bundled MPFR's optional host float128 detection conflicts with current
# glibc under Clang. This does not change target long-double/SoftFloat support.
if ! grep -q -- '--disable-float128' "$source_dir/scripts/002-gcc-newlib-PPU.sh"; then
    patch "$source_dir/scripts/002-gcc-newlib-PPU.sh" <<'PATCH'
@@ -61,2 +61,3 @@
     --disable-dependency-tracking \
+    --disable-float128 \
     --disable-libcc1 \
PATCH
fi
cd "$source_dir/scripts"
bash ./002-gcc-newlib-PPU.sh
