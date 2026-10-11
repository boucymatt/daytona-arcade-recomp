// Allocation for the single-threaded, exception-disabled probe ONLY.
// Newlib's Xbox allocator has no _memalign_r; retain the original malloc
// pointer for all allocations so aligned delete always calls free correctly.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

static void *allocate(std::size_t size, std::size_t alignment) noexcept {
    if (alignment < alignof(void *)) alignment = alignof(void *);
    if ((alignment & (alignment - 1)) != 0) return nullptr;
    if (size == 0) size = 1;
    const auto limit = SIZE_MAX;
    if (alignment - 1 > limit - sizeof(void *)) return nullptr;
    const auto overhead = alignment - 1 + sizeof(void *);
    if (size > limit - overhead) return nullptr;
    void *raw = std::malloc(size + overhead);
    if (!raw) return nullptr;
    auto address = (reinterpret_cast<std::uintptr_t>(raw) + sizeof(void *) + alignment - 1)
                   & ~(std::uintptr_t(alignment) - 1);
    auto result = reinterpret_cast<void **>(address);
    result[-1] = raw;
    return result;
}
static void release(void *pointer) noexcept {
    if (pointer) std::free(static_cast<void **>(pointer)[-1]);
}
void *operator new(std::size_t n) {
    if (void *p = allocate(n, alignof(std::max_align_t))) return p;
    std::abort(); // No exceptions in this probe; never return null from throwing new.
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
    return allocate(n, alignof(std::max_align_t));
}
void *operator new[](std::size_t n, const std::nothrow_t &tag) noexcept { return ::operator new(n, tag); }
void *operator new(std::size_t n, std::align_val_t alignment) {
    if (void *p = allocate(n, static_cast<std::size_t>(alignment))) return p;
    std::abort();
}
void *operator new[](std::size_t n, std::align_val_t alignment) { return ::operator new(n, alignment); }
void *operator new(std::size_t n, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return allocate(n, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t &tag) noexcept {
    return ::operator new(n, a, tag);
}
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, std::size_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t) noexcept { release(p); }
void operator delete(void *p, std::align_val_t) noexcept { release(p); }
void operator delete[](void *p, std::align_val_t) noexcept { release(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { release(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { release(p); }
