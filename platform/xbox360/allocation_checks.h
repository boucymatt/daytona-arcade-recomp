#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>

// Exercise the same allocation implementation on host and console.
inline bool allocation_checks() {
    for (std::size_t alignment = 8; alignment <= 4096; alignment *= 2) {
        for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{31}, std::size_t{257}}) {
            auto tag = static_cast<std::align_val_t>(alignment);
            void *p = ::operator new(size, tag, std::nothrow);
            if (!p) return false;
            bool aligned = reinterpret_cast<std::uintptr_t>(p) % alignment == 0;
            std::memset(p, 0xa5, size ? size : 1);
            ::operator delete(p, tag);
            if (!aligned) return false;
        }
    }
    void *too_large = ::operator new(SIZE_MAX, std::nothrow);
    if (too_large) { ::operator delete(too_large); return false; }
    return true;
}
