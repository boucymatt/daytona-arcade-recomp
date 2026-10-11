// Compiler-generated TLS addressing backed by the isolated Xbox TLS adapter.
#include <stdint.h>
#include <stdio.h>
#include <xecore/xboxkrnl_threads.h>
#include <xecore/xboxkrnl_io.h>
#include "tls_runtime.h"

static thread_local uint32_t zero_value;
static thread_local uint32_t initial_value = 0x12345678;
struct alignas(64) Aligned { uint32_t value; };
static thread_local Aligned aligned_value;
static uint32_t constructions, destructions;
struct Lifetime {
    uint32_t value;
    Lifetime() : value(73) { __atomic_fetch_add(&constructions, 1, __ATOMIC_RELAXED); }
    ~Lifetime() { __atomic_fetch_add(&destructions, 1, __ATOMIC_RELAXED); }
};
static thread_local Lifetime lifetime;
struct Worker { uint32_t expected, passed; uintptr_t address; };
static Worker workers[2];
static uint32_t ready;

static uint32_t worker_main(void *argument) {
    Worker *w = static_cast<Worker *>(argument);
    bool ok = zero_value == 0 && initial_value == 0x12345678 && lifetime.value == 73;
    ok &= reinterpret_cast<uintptr_t>(&aligned_value) % 64 == 0;
    w->address = reinterpret_cast<uintptr_t>(&zero_value);
    zero_value = w->expected;
    initial_value = w->expected + 1;
    aligned_value.value = w->expected + 2;
    lifetime.value = w->expected + 3;
    __atomic_fetch_add(&ready, 1, __ATOMIC_RELEASE);
    unsigned waited = 0;
    while (__atomic_load_n(&ready, __ATOMIC_ACQUIRE) < 2 && waited++ < 1000) {
        int64_t delay = -10000;
        KeDelayExecutionThread(0, 0, &delay);
    }
    ok &= __atomic_load_n(&ready, __ATOMIC_ACQUIRE) == 2;
    for (unsigned i = 0; i < 64; ++i) {
        int64_t delay = -10000;
        KeDelayExecutionThread(0, 0, &delay);
        ok &= zero_value == w->expected && initial_value == w->expected + 1 &&
              aligned_value.value == w->expected + 2 && lifetime.value == w->expected + 3;
    }
    xbox_tls_thread_cleanup();
    __atomic_store_n(&w->passed, ok ? 1u : 2u, __ATOMIC_RELEASE);
    ExTerminateThread(ok ? 0 : 1);
    return 0;
}

int main() {
    FILE *log = fopen("game:\\xbox360-tls.log", "wb");
    if (!log) return 1;
    fputs("Compiler TLS probe: bounded Xbox adapter; exceptions NOT tested\n", log);
    fflush(log);
    if (!xbox_tls_init()) { fputs("TLS init FAIL\n", log); fclose(log); return 1; }
    bool ok = zero_value == 0 && initial_value == 0x12345678 && lifetime.value == 73;
    zero_value = 0xaabbccdd;
    uintptr_t main_address = reinterpret_cast<uintptr_t>(&zero_value);
    fprintf(log, "Main initial values %s\n", ok ? "PASS" : "FAIL");
    fflush(log);
    HANDLE handles[2] = {};
    unsigned created = 0, joined = 0;
    for (unsigned i = 0; i < 2; ++i) {
        workers[i].expected = 100 + i * 10;
        uint32_t id = 0;
        NTSTATUS result = ExCreateThread(&handles[i], 0x10000, &id, nullptr,
                                         reinterpret_cast<void *>(worker_main), &workers[i], 0);
        fprintf(log, "Create worker=%u status=%08lx\n", i, (unsigned long)result);
        fflush(log);
        if (result != 0 || !handles[i]) { ok = false; break; }
        ++created;
    }
    for (unsigned i = 0; i < created; ++i) {
        int64_t timeout = -50000000;
        NTSTATUS result = NtWaitForSingleObjectEx((uint32_t)(uintptr_t)handles[i], 0, 0, &timeout);
        fprintf(log, "Join worker=%u status=%08lx\n", i, (unsigned long)result);
        if (result == 0) {
            ++joined;
            bool pass = __atomic_load_n(&workers[i].passed, __ATOMIC_ACQUIRE) == 1;
            fprintf(log, "Worker=%u initialization/alignment/isolation %s\n", i, pass ? "PASS" : "FAIL");
            ok &= pass;
        } else ok = false;
        ok &= NtClose(handles[i]) == 0;
        fflush(log);
    }
    if (joined != created) {
        fputs("TLS probe incomplete: join FAIL; retain TLS until process teardown\n", log);
        fclose(log);
        return 1;
    }
    bool distinct = created == 2 && workers[0].address != workers[1].address &&
                    workers[0].address != main_address && workers[1].address != main_address;
    ok &= distinct && zero_value == 0xaabbccdd && initial_value == 0x12345678 && lifetime.value == 73;
    fprintf(log, "Distinct addresses and main isolation %s\n", ok ? "PASS" : "FAIL");
    xbox_tls_thread_cleanup();
    uint32_t made = __atomic_load_n(&constructions, __ATOMIC_ACQUIRE);
    uint32_t destroyed = __atomic_load_n(&destructions, __ATOMIC_ACQUIRE);
    fprintf(log, "TLS constructors=%lu destructors=%lu expected=3/3\n",
            (unsigned long)made, (unsigned long)destroyed);
    ok &= made == 3 && destroyed == 3;
    ok &= xbox_tls_shutdown() != 0;
    fprintf(log, "Compiler TLS probe complete: %s\n", ok ? "PASS" : "FAIL");
    return fclose(log) == 0 && ok ? 0 : 1;
}
