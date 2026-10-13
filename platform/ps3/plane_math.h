#pragma once
#include <cmath>
#include <limits>

namespace ps3 {
// All finite float squares fit in double, including subnormal inputs. In the
// clipper's integer viewport domain, 1 + plane*plane is exact in double.
// Avoid the target library's scaled single-precision hypotf approximation.
inline float plane_hypot(float x, float y) {
    if (std::isinf(x) || std::isinf(y)) return std::numeric_limits<float>::infinity();
    const double dx = x, dy = y;
    return float(std::sqrt(dx * dx + dy * dy));
}
}
