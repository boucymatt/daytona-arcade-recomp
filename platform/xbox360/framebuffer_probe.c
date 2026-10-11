/* Physical buffer prerequisite only: no GPU submissions or display writes. */
#include <stdio.h>
#include <stdint.h>
#include <xecore/xboxkrnl_mem.h>
#include <xecore/xboxkrnl_threads.h>

enum { WIDTH = 640, HEIGHT = 480, BYTES = WIDTH * HEIGHT * 4, PAGE = 4096 };

/* Explicit byte pattern, independent of CPU endian and unvalidated GPU format. */
static unsigned char pattern(unsigned offset) {
    unsigned pixel = offset / 4, x = pixel % WIDTH, y = pixel / WIDTH;
    switch (offset % 4) {
    case 0: return (unsigned char)x;
    case 1: return (unsigned char)y;
    case 2: return (unsigned char)(x ^ y);
    default: return 255;
    }
}

int main(void) {
    FILE *log = fopen("game:\\xbox360-framebuffer.log", "wb");
    if (log) {
        fputs("Physical framebuffer probe revision 1; no GPU commands; no automatic exit\n", log);
        fprintf(log, "Allocation %u x %u, %u bytes; alignment=%u\n", WIDTH, HEIGHT, BYTES, PAGE);
        fputs("Before physical allocation\n", log);
        if (fflush(log) == 0) {
            void *allocation = MmAllocatePhysicalMemoryEx(0, BYTES,
                PAGE_READWRITE | PAGE_WRITECOMBINE, 0, UINT32_MAX, PAGE);
            if (!allocation) {
                fputs("FAIL: allocation returned null\n", log);
            } else {
                uint32_t physical = MmGetPhysicalAddress(allocation);
                int ok = physical != UINT32_MAX && (physical % PAGE) == 0;
                fprintf(log, "Allocated virtual=%08x physical=%08x\n",
                        (unsigned)(uintptr_t)allocation, (unsigned)physical);
                volatile unsigned char *bytes = allocation;
                for (unsigned offset = 0; offset < BYTES; offset += PAGE) {
                    uint32_t address = MmGetPhysicalAddress((unsigned char *)allocation + offset);
                    if (address != physical + offset) { ok = 0; break; }
                }
                fprintf(log, "Physical page mapping %s\n", ok ? "PASS" : "FAIL");
                if (ok && fflush(log) == 0) {
                    for (unsigned i = 0; i < BYTES; ++i) bytes[i] = pattern(i);
                    __asm__ volatile("sync" ::: "memory");
                    unsigned mismatches = 0;
                    for (unsigned i = 0; i < BYTES; ++i)
                        mismatches += bytes[i] != pattern(i);
                    fprintf(log, "Byte readback mismatches=%u %s\n", mismatches,
                            mismatches ? "FAIL" : "PASS");
                    ok &= mismatches == 0;
                } else ok = 0;
                fputs("Before physical free\n", log); fflush(log);
                MmFreePhysicalMemory(0, allocation);
                fprintf(log, "After physical free; buffer test %s\n", ok ? "PASS" : "FAIL");
            }
        }
        fputs("Presentation NOT tested; parking; leave through Guide\n", log);
        fclose(log);
    }
    for (;;) {
        int64_t interval = -10000000;
        KeDelayExecutionThread(0, 0, &interval);
    }
}
