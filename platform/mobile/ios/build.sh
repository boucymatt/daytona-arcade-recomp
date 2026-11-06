#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build/ios}"
GEN="${M2_GEN_ROOT:-$ROOT/build/gen}"
REV_GEN="${M2_REV_GEN_ROOT:-$ROOT/build-daytona/gen}"
UNSIGNED="${UNSIGNED:-0}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "iOS builds require macOS with Xcode." >&2
    exit 1
fi
check_sources() {
    local gen_root="$1" romset="$2" source
    for source in "$romset/gen_table.cpp" "${romset}_tgp/tgp_gen.cpp" "${romset}_snd/snd_gen.cpp"; do
        if [[ ! -f "$gen_root/$source" ]]; then
            echo "Missing generated game source: $gen_root/$source" >&2
            echo "Recompile both ROM sets on the host before building the mobile app (see platform/mobile/README.md)." >&2
            exit 1
        fi
    done
}
check_sources "$GEN" daytona93
check_sources "$REV_GEN" daytona

python3 "$ROOT/scripts/setup.py" --no-build

mkdir -p "$BUILD"
BUILD="$(cd "$BUILD" && pwd)"
GEN="$(cd "$GEN" && pwd)"
REV_GEN="$(cd "$REV_GEN" && pwd)"
set -- -S "$ROOT" -B "$BUILD" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DM2_ROMSET=daytona93 \
    -DM2_MULTI_ROMSETS=ON -DM2_GEN_ROOT="$GEN" -DM2_REV_GEN_ROOT="$REV_GEN"
if [[ "$UNSIGNED" == 1 ]]; then
    set -- "$@" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=
fi
cmake "$@"

cmake --build "$BUILD" --config Release --target daytona
echo
echo "Built: $BUILD/Release-iphoneos/daytona.app"
if [[ "$UNSIGNED" == 1 ]]; then
    STAGE="$(mktemp -d "$BUILD/ipa.XXXXXX")"
    mkdir "$STAGE/Payload"
    ditto --norsrc --noextattr --noqtn "$BUILD/Release-iphoneos/daytona.app" "$STAGE/Payload/daytona.app"
    if codesign -d "$STAGE/Payload/daytona.app" >/dev/null 2>&1; then
        codesign --remove-signature "$STAGE/Payload/daytona.app"
    fi
    /usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$STAGE/Payload/daytona.app/Info.plist"
    (cd "$STAGE" && ditto -c -k --norsrc --noextattr --noqtn --keepParent Payload "$BUILD/Daytona-unsigned.ipa")
    echo "Unsigned IPA: $BUILD/Daytona-unsigned.ipa (sign with AltStore before installing)"
else
    echo "Open $BUILD/daytona_recomp.xcodeproj to select your signing team and deploy."
fi
