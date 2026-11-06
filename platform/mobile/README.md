# Android / iOS port

This port keeps the desktop runtime and SDL3/SDL_GPU renderer. Each Android or
iOS app contains both Daytona USA Deluxe '93 (`daytona93`) and Revision A
(`daytona`, 1994). Browse to your ROM archive in the launcher: the app detects
the set and uses its recompiled program, with separate settings and saves for
each version. Both versions run within the same installed app.
Use the launcher's **Version** selector to return to either version and its
saved ROM path. The app remembers the last selected version on relaunch.

## Prerequisite: generate the game code on a desktop host

ROM-derived C++ is never committed. Put both ROM archives in `roms/` and
build/recompile on the host so these exist:

- `build/gen/daytona93/*.cpp`
- `build/gen/daytona93_tgp/tgp_gen.cpp`
- `build/gen/daytona93_snd/snd_gen.cpp`
- `build-daytona/gen/daytona/*.cpp`
- `build-daytona/gen/daytona_tgp/tgp_gen.cpp`
- `build-daytona/gen/daytona_snd/snd_gen.cpp`

Normally:

```sh
./setup.sh
```

Setup generates both sets when both archives are present. To regenerate them
after changing a recompiler or its seeds:

```sh
python3 scripts/recompile.py
python3 scripts/recompile.py --set daytona --build-dir build-daytona
```

The mobile builds read those directories through `M2_GEN_ROOT` and
`M2_REV_GEN_ROOT`, and enable `M2_MULTI_ROMSETS`. They require generated i960,
TGP and sound sources for both sets. No ROM archive is included in the app.

## Android

GPU startup failures now show a native **Daytona Error** dialog and log the
compiled GPU drivers. Vulkan API version alone does not establish SDL_GPU
compatibility. Startup opts out of unused shader clip distances, indirect
first-instance drawing and anisotropic filtering; depth clamping remains
required by the current game and ImGui pipelines. This is not an OpenGL
fallback, nor a guarantee for every Mali/Adreno driver.

If startup still fails, capture the GPU diagnostics (not only SDL/APP):

```sh
adb logcat -d -s SDL SDL/APP SDL/GPU AndroidRuntime libc
```

Requirements: JDK 17, Android SDK API 36, NDK 28.2.13676358, CMake 3.31.6, and
the Gradle 9.1.0 wrapper (Android Gradle Plugin 9.0.0).

```sh
python3 scripts/setup.py --no-build
cd platform/mobile/android
bash gradlew :app:assembleDebug
```

The APK is under `app/build/outputs/apk/debug/`.

The Android package uses SDL3's `SDLActivity`, builds SDL as `libSDL3.so`, and loads the
game CMake target as `libmain.so`. It is arm64-only for now. SDL handles Bluetooth/USB
gamepads and the SDL file dialog is used by the existing launcher for selecting a user
provided ZIP/7z ROM set.

For generated sources in different directories, pass Gradle properties
`-PM2_GEN_ROOT=/path/to/deluxe/gen` and
`-PM2_REV_GEN_ROOT=/path/to/revision-a/gen`, or set environment variables with
those names. Properties take precedence; relative paths are resolved from
`platform/mobile/android`.

## iOS

Requirements: macOS, Xcode, CMake, and an Apple signing identity for installation on a
physical device.

```sh
platform/mobile/ios/build.sh
```

That generates an Xcode iOS project and builds `daytona.app`. Open the generated
`daytona_recomp.xcodeproj`, select your Team under Signing, then deploy to the device.
The bundle opts into Files document sharing/open-in-place so the user's ROM archive can
be made available to the launcher.
On iOS, Browse opens the native Files document picker and imports a private copy
before the existing ROM checks. Choose either set's ZIP/7z from On My iPhone,
iCloud Drive, or another Files provider. Each selection keeps a separate copy,
so an invalid selection cannot overwrite an earlier import. The launcher uses
the screen safe area; drag blank space vertically or use its right scrollbar
to reach the remaining options and Start button.

For an unsigned IPA to sign and install through AltStore:

```sh
UNSIGNED=1 platform/mobile/ios/build.sh
```

The output is `build/ios/Daytona-unsigned.ipa`, containing the arm64 iPhoneOS app
inside `Payload/daytona.app`. This mode disables Xcode code signing; AltStore
must sign the IPA before installation. It does not bundle a ROM archive. Set
`M2_GEN_ROOT` if Deluxe '93's generated sources are outside `build/gen`,
`M2_REV_GEN_ROOT` if Revision A's are outside `build-daytona/gen`, and
`BUILD_DIR` to select a different output directory. Device testing on
2026-10-06 covered the earlier Deluxe '93 app; switching between versions
requires a new device check.

## Current input

SDL3 gamepads work on both platforms using the same bindings as desktop. The launcher is
touch/mouse compatible through SDL/ImGui. iOS and Android show a shared touch overlay
during gameplay:

- Slide across STEER at the lower left for analogue steering; the middle is neutral.
- Hold GAS or BRAKE at the lower right. Different fingers can steer and use pedals together.
- Gear -/+ shift sequentially; V1-V4 select the four views.
- Coin and Start operate the arcade buttons. Menu pauses and returns to the launcher.

Controls scale with the safe area. Releasing a finger, changing the layout, pausing,
or backgrounding the app clears held input. Brief button taps are retained until
the next game frame. Touch and physical controls can be used together. Touch
steering uses the existing invert setting; buttons are full-travel pedals.
