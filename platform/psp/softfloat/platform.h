// Project-owned SoftFloat configuration for little-endian 32-bit Allegrex.
// The target has no __int128; retain the portable 64-bit integer helpers.
#pragma once
#define LITTLEENDIAN 1
#ifdef __GNUC_STDC_INLINE__
#define INLINE inline
#else
#define INLINE extern inline
#endif
#define SOFTFLOAT_BUILTIN_CLZ 1
#include "opts-GCC.h"
