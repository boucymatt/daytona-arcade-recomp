// Bounded bring-up adapter for Clang -femulated-tls, not a general ABI library.
// Descriptor layout verified against the pinned compiler-rt emutls ABI.
// No heap calls: the current Xbox newlib allocator is not thread-safe.
#include "tls_runtime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef XBOX_TLS_HOST_TEST
#include <xecore/xboxkrnl_threads.h>
#include <xecore/xboxkrnl_debug.h>
#else
extern "C" uintptr_t tls_test_get(void);
extern "C" int tls_test_set(uintptr_t);
#endif

struct Control { uintptr_t size, alignment, reserved; const void *initial; };
struct Entry { const Control *control; void *address; };
struct Destructor { void (*call)(void *); void *object; };
struct Context {
    uint32_t claimed;
    unsigned entries, used, destructors;
    Entry objects[32];
    Destructor dtors[32];
    alignas(256) unsigned char arena[16384];
};
static Context contexts[4];
static uint32_t slot = UINT32_MAX;
static bool initialized;

[[noreturn]] static void fail() {
#ifndef XBOX_TLS_HOST_TEST
    DbgPrint("Daytona TLS adapter: capacity, lifecycle or kernel failure\n");
#endif
    abort(); // Never return an invalid pointer to compiler-generated TLS code.
}
static Context *current() {
#ifdef XBOX_TLS_HOST_TEST
    return reinterpret_cast<Context *>(tls_test_get());
#else
    return reinterpret_cast<Context *>((uintptr_t)KeTlsGetValue(slot));
#endif
}
static bool set_current(Context *context) {
#ifdef XBOX_TLS_HOST_TEST
    return tls_test_set(reinterpret_cast<uintptr_t>(context)) != 0;
#else
    return KeTlsSetValue(slot, (uint32_t)(uintptr_t)context) != 0;
#endif
}
static Context *get_context() {
    if (!initialized) fail();
    if (Context *context = current()) return context;
    for (Context &context : contexts) {
        uint32_t expected = 0;
        if (__atomic_compare_exchange_n(&context.claimed, &expected, 1, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            if (!set_current(&context)) {
                __atomic_store_n(&context.claimed, 0, __ATOMIC_RELEASE);
                fail();
            }
            return &context;
        }
    }
    fail();
}
extern "C" int xbox_tls_init() {
    if (initialized) return 0; // Caller must serialize init/shutdown.
#ifdef XBOX_TLS_HOST_TEST
    slot = 0;
#else
    slot = KeTlsAlloc();
#endif
    if (slot == UINT32_MAX) return 0;
    initialized = true;
    return 1;
}
extern "C" void *__emutls_get_address(Control *control) {
    Context *context = get_context();
    for (unsigned i = 0; i < context->entries; ++i)
        if (context->objects[i].control == control) return context->objects[i].address;
    uintptr_t alignment = control->alignment;
    if (!alignment || (alignment & (alignment - 1)) || alignment > 256 ||
        context->entries == 32 || control->size == 0) fail();
    uintptr_t offset = (context->used + alignment - 1) & ~(alignment - 1);
    if (offset > sizeof(context->arena) || control->size > sizeof(context->arena) - offset) fail();
    void *address = context->arena + offset;
    if (control->initial) memcpy(address, control->initial, control->size);
    else memset(address, 0, control->size);
    context->objects[context->entries++] = {control, address};
    context->used = static_cast<unsigned>(offset + control->size);
    return address;
}
extern "C" int __cxa_thread_atexit(void (*destructor)(void *), void *object, void *) {
    Context *context = get_context();
    if (!destructor || context->destructors == 32) fail();
    context->dtors[context->destructors++] = {destructor, object};
    return 0;
}
extern "C" void xbox_tls_thread_cleanup() {
    if (!initialized) fail();
    Context *context = current();
    if (!context) return;
    unsigned calls = 0;
    while (context->destructors) {
        if (++calls > 128) fail(); // Bound destructor re-registration.
        Destructor dtor = context->dtors[--context->destructors];
        dtor.call(dtor.object);
    }
    context->entries = context->used = 0;
    if (!set_current(nullptr)) fail();
    __atomic_store_n(&context->claimed, 0, __ATOMIC_RELEASE);
}
extern "C" int xbox_tls_shutdown() {
    if (!initialized) return 0;
    for (Context &context : contexts)
        if (__atomic_load_n(&context.claimed, __ATOMIC_ACQUIRE)) return 0;
#ifndef XBOX_TLS_HOST_TEST
    if (!KeTlsFree(slot)) return 0;
#endif
    slot = UINT32_MAX;
    initialized = false;
    return 1;
}
#ifndef XBOX_TLS_HOST_TEST
// Static XEX only; unloading dynamic modules is outside this adapter's scope.
extern "C" { void *__dso_handle = &__dso_handle; }
static_assert(sizeof(Control) == 16, "Expected 32-bit compiler emutls descriptor");
#endif
