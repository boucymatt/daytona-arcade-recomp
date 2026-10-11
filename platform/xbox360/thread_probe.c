/* Native kernel threading gate, not std::thread or compiler thread_local.
   Workers use no newlib allocation/stdio: the SDK heap is not thread-safe. */
#include <stdint.h>
#include <stdio.h>
#include <xecore/xboxkrnl_threads.h>
#include <xecore/xboxkrnl_io.h>

_Static_assert(sizeof(void *) == 4, "Xbox ABI requires 32-bit pointers");
_Static_assert(__atomic_always_lock_free(4, 0), "Probe requires lock-free 32-bit atomics");

typedef struct {
    uint32_t slot, sentinel, initial, final, set_ok, completed;
} Worker;
static Worker workers[2];
static uint32_t counter;

static uint32_t worker_main(void *context) {
    Worker *worker = context;
    worker->initial = KeTlsGetValue(worker->slot);
    worker->set_ok = KeTlsSetValue(worker->slot, worker->sentinel);
    for (unsigned batch = 0; batch < 64; ++batch) {
        for (unsigned i = 0; i < 256; ++i)
            __atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);
        int64_t pause = -10000; /* 1 ms; permit overlap without busy waiting. */
        KeDelayExecutionThread(0, 0, &pause);
        if (KeTlsGetValue(worker->slot) != worker->sentinel) {
            worker->final = KeTlsGetValue(worker->slot);
            __atomic_store_n(&worker->completed, 2, __ATOMIC_RELEASE);
            ExTerminateThread(1);
            return 1;
        }
    }
    worker->final = KeTlsGetValue(worker->slot);
    __atomic_store_n(&worker->completed, 1, __ATOMIC_RELEASE);
    ExTerminateThread(0);
    return 0;
}

int main(void) {
    FILE *log = fopen("game:\\xbox360-thread.log", "wb");
    if (!log) return 1;
    fputs("Native threads/atomic/kernel TLS probe; C++ exceptions and compiler TLS NOT tested\n", log);
    fflush(log);
    uint32_t slot = KeTlsAlloc();
    fprintf(log, "TLS slot=%lu\n", (unsigned long)slot);
    fflush(log);
    if (slot == UINT32_MAX) {
        fputs("TLS allocation FAIL\n", log);
        fclose(log);
        return 1;
    }
    const uint32_t main_value = 0x13579bdf;
    int ok = KeTlsSetValue(slot, main_value) != 0 && KeTlsGetValue(slot) == main_value;
    HANDLE handles[2] = {0};
    unsigned created = 0, joined = 0;
    if (!ok) {
        fputs("Main TLS round-trip FAIL\n", log);
        KeTlsFree(slot);
        fclose(log);
        return 1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        workers[i].slot = slot;
        workers[i].sentinel = 0x24680000u + i;
        uint32_t id = 0;
        NTSTATUS status = ExCreateThread(&handles[i], 0x10000, &id, 0,
                                        (void *)worker_main, &workers[i], 0);
        fprintf(log, "Create worker=%u status=%08lx id=%lu\n", i,
                (unsigned long)status, (unsigned long)id);
        fflush(log);
        if (status != 0 || !handles[i]) { ok = 0; break; }
        ++created;
    }
    for (unsigned i = 0; i < created; ++i) {
        int64_t timeout = -50000000; /* Bound each join to five seconds. */
        NTSTATUS status = NtWaitForSingleObjectEx((uint32_t)(uintptr_t)handles[i], 0, 0, &timeout);
        fprintf(log, "Join worker=%u status=%08lx\n", i, (unsigned long)status);
        if (status == 0) {
            ++joined;
            uint32_t done = __atomic_load_n(&workers[i].completed, __ATOMIC_ACQUIRE);
            int passed = done == 1 && workers[i].initial == 0 && workers[i].set_ok != 0 &&
                         workers[i].final == workers[i].sentinel;
            fprintf(log, "Worker=%u initial=%08lx final=%08lx TLS %s\n", i,
                    (unsigned long)workers[i].initial, (unsigned long)workers[i].final,
                    passed ? "PASS" : "FAIL");
            ok &= passed;
        } else ok = 0;
        NTSTATUS closed = NtClose(handles[i]);
        fprintf(log, "Close worker=%u status=%08lx\n", i, (unsigned long)closed);
        ok &= closed == 0;
        fflush(log);
    }
    uint32_t total = __atomic_load_n(&counter, __ATOMIC_ACQUIRE);
    int main_preserved = KeTlsGetValue(slot) == main_value;
    fprintf(log, "Main TLS isolation %s\n", main_preserved ? "PASS" : "FAIL");
    fprintf(log, "Atomic counter=%lu expected=32768\n", (unsigned long)total);
    ok &= created == 2 && joined == 2 && total == 32768 && main_preserved;
    // Never free a slot that an unjoined worker may still access. Process
    // teardown owns cleanup in this failure case; worker contexts are static.
    if (joined == created) {
        uint32_t freed = KeTlsFree(slot);
        fprintf(log, "TLS free=%lu\n", (unsigned long)freed);
        ok &= freed != 0;
    }
    fprintf(log, "Thread probe complete: %s\n", ok ? "PASS" : "FAIL");
    return fclose(log) == 0 && ok ? 0 : 1;
}
