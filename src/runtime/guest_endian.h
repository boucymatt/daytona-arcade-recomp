// Byte-backed Model 2 memory is little-endian on every host.
#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace rt {
template<class T> inline T guest_load(const void *address) {
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>);
    T value;
    std::memcpy(&value, address, sizeof(value));
    if constexpr (std::endian::native == std::endian::big) {
        if constexpr (sizeof(T) == 2) value = T((value >> 8) | (value << 8));
        else value = T((value >> 24) | ((value >> 8) & 0xff00u) |
                       ((value << 8) & 0xff0000u) | (value << 24));
    }
    return value;
}
template<class T> inline void guest_store(void *address, T value) {
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>);
    if constexpr (std::endian::native == std::endian::big) {
        auto *bytes = static_cast<uint8_t *>(address);
        for (unsigned i = 0; i < sizeof(T); ++i) bytes[i] = uint8_t(value >> (8 * i));
    } else std::memcpy(address, &value, sizeof(value));
}
template<class T> inline void guest_copy_words(T *out, const uint8_t *in, size_t count) {
    if (!count) return;
    if constexpr (std::endian::native == std::endian::little) std::memcpy(out, in, count * sizeof(T));
    else for (size_t i = 0; i < count; ++i) out[i] = guest_load<T>(in + i * sizeof(T));
}
}
