# PSP build dependencies

This frontend uses the public, homebrew PSPDEV/PSPSDK toolchain, not a Sony SDK.
The sources and game-derived build output remain separate. No firmware or
third-party repository is copied into this source tree.

| Component | Upstream / revision used | Licence | Use and changes |
| --- | --- | --- | --- |
| PSPSDK | https://github.com/pspdev/pspsdk at `887b07fed3b97d495409851712e6ea45c7643720` | BSD-3-Clause | Native GU, audio, controller, power, kernel, libc glue and PBP packaging APIs. Linked unmodified; project frontend written against its public headers. |
| PSPDEV | https://github.com/pspdev/pspdev at `df74d7a30a84a12fbde5fdae9f5fa5b350bc77d9` | Build recipes plus individual tool licences | Installed cross-toolchain; no copied code or modified tools. |
| GCC / libstdc++ 15.2.0 | https://github.com/pspdev/gcc at `1a33997924916ff5a6f61b64179ff9c8921f46c6` | GPL-3.0 with GCC Runtime Library Exception for runtime libraries | Cross-compiler and C++ runtime; unmodified. |
| newlib | https://github.com/pspdev/newlib at `9e0a073634ad73e8e088f2e071c55a9fe5d39709` | Per-file permissive licences | Installed PSP C runtime; unmodified. |
| PPSSPP 1.20.4 | https://github.com/hrydgard/ppsspp/tree/v1.20.4 | GPL-2.0-or-later | Development emulator only, not linked, bundled or required on the PSP. |

Shared runtime dependencies and notices are in [THIRD_PARTY.md](../../THIRD_PARTY.md).
These permissive SDK and runtime-exception dependencies do not impose a new
copyleft licence on the project's own frontend. The project's final licence
remains undecided; ROM-derived packages are private user builds.

## PSPSDK licence

Copyright (c) 2005  adresd
Copyright (c) 2005  Marcus R. Brown
Copyright (c) 2005  James Forshaw
Copyright (c) 2005  John Kelley
Copyright (c) 2005  Jesper Svennevid
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The names of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
