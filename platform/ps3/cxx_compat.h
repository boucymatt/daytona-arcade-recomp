#pragma once
// ps3toolchain GCC 13.2/newlib 1.20 supplies these C99 entry points, but
// libstdc++'s configure probes do not expose their C++ namespace overloads.
// Isolated target shim; do not enable on other toolchains.
#include <cmath>
#include <cstdio>
#include "plane_math.h"
namespace std {
using ::snprintf;
using ::vsnprintf;
using ::round;
using ::trunc;
using ::hypot;
inline float hypot(float x, float y) { return ps3::plane_hypot(x, y); }
}
