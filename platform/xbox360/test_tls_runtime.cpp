// Host tests for adapter behavior; target ABI is tested separately on Xbox.
#include "tls_runtime.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

struct Control { uintptr_t size, alignment, reserved; const void *initial; };
extern "C" void *test_emutls_address(Control *);
extern "C" int test_thread_atexit(void (*)(void *), void *, void *);
static thread_local uintptr_t context;
static thread_local unsigned order, count;
extern "C" uintptr_t tls_test_get() { return context; }
extern "C" int tls_test_set(uintptr_t value) { context = value; return 1; }
static void destroy(void *p) { order = order * 10 + *static_cast<unsigned *>(p); ++count; }
static uint32_t initial = 0x12345678;
static Control value{4, 4, 0, &initial}, zero{256, 256, 0, nullptr};
static std::atomic<unsigned> ready;

static void exercise(unsigned sentinel) {
    for (unsigned repeat = 0; repeat < 100; ++repeat) {
        auto *number = static_cast<uint32_t *>(test_emutls_address(&value));
        assert(*number == initial);
        assert(test_emutls_address(&value) == number);
        auto *bytes = static_cast<unsigned char *>(test_emutls_address(&zero));
        assert(reinterpret_cast<uintptr_t>(bytes) % 256 == 0);
        for (unsigned i = 0; i < 256; ++i) assert(bytes[i] == 0);
        *number = sentinel;
        std::memset(bytes, 0x5a, 256);
        if (repeat == 0) {
            ready.fetch_add(1);
            while (ready.load() < 2) std::this_thread::yield();
        }
        std::this_thread::yield();
        assert(*number == sentinel);
        unsigned first = 1, second = 2;
        order = count = 0;
        assert(test_thread_atexit(destroy, &first, nullptr) == 0);
        assert(test_thread_atexit(destroy, &second, nullptr) == 0);
        xbox_tls_thread_cleanup();
        assert(order == 21 && count == 2 && context == 0);
    }
}
int main(int argc, char **argv) {
    assert(xbox_tls_init());
    assert(!xbox_tls_init());
    if (argc == 2) {
        Control invalid{16385, 4, 0, nullptr};
        if (std::strcmp(argv[1], "alignment") == 0) invalid = {4, 3, 0, nullptr};
        test_emutls_address(&invalid);
        return 9; // Invalid descriptors must abort, never return.
    }
    auto *main_value = static_cast<uint32_t *>(test_emutls_address(&value));
    *main_value = 99;
    assert(!xbox_tls_shutdown()); // Refuse shutdown with an attached thread.
    std::thread a(exercise, 10), b(exercise, 20);
    a.join(); b.join();
    assert(*main_value == 99);
    xbox_tls_thread_cleanup();
    assert(xbox_tls_shutdown());
    assert(!xbox_tls_shutdown());
    assert(xbox_tls_init()); // A new lifecycle must not retain old values.
    assert(*static_cast<uint32_t *>(test_emutls_address(&value)) == initial);
    xbox_tls_thread_cleanup();
    assert(xbox_tls_shutdown());
    std::puts("TLS adapter PASS: 200 worker lifecycles, isolation, alignment, reverse destructors, shutdown");
}
