# Xbox 360 port: bring-up, not a playable build

Target: RGH Xbox 360 S (Trinity), launched from Aurora. This is an isolated
OpenXeChain target; desktop/mobile/Vita build definitions are unchanged.
No proprietary SDK is used. No console runtime results have been recorded.

**Build result (2026-10-10):** boot and input probes compile/link/package with
the pinned OpenXeChain SDK plus the completion step below. Each XEX is 260 KiB.
PowerPC/32-bit/Xbox PE headers and XEX2 signatures were checked. The toolchain
emits `/align specified without /driver; image may not run`; this warning is
recorded, not dismissed as proof of a valid hardware launch. The C++ gate
fails with `<vector> file not found`. Neither probe runs Daytona.

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
