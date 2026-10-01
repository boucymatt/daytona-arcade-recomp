#pragma once
#include <cstddef>

namespace rt {
// Optional owner-thread diagnostics. Never controls guest timing or pixels.
using RenderObserver = void (*)(void*, const char*, size_t, size_t);
} // namespace rt
