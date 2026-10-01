#pragma once
#include <array>
#include <cstddef>

namespace psp {
struct RomCachePolicy { size_t bytes; unsigned ways; };
// Same 832 KiB main-board budget; favor texture reuse over cold program pages.
// Audio retains its separate 512 KiB budget and existing associativity.
inline constexpr std::array<RomCachePolicy, 7> kRomCaches{{
    {128 * 1024, 4}, {128 * 1024, 16}, {64 * 1024, 8},
    {256 * 1024, 16}, {256 * 1024, 4},
    {256 * 1024, 4}, {256 * 1024, 4}
}};
static_assert(kRomCaches[0].bytes + kRomCaches[1].bytes + kRomCaches[2].bytes +
              kRomCaches[3].bytes + kRomCaches[4].bytes == 832 * 1024);
static_assert(kRomCaches[5].bytes + kRomCaches[6].bytes == 512 * 1024);
} // namespace psp
