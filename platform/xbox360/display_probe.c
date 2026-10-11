/* Read-only display ABI probe; no GPU initialization or register writes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xecore/xboxkrnl_threads.h>

/* xecorelib's video header omits this argument. ABI reference: Xenia
 * 95a5c3ee, xboxkrnl_video.cc, X_DISPLAY_INFO (0x58 bytes).
 * Independent declaration; no emulator implementation is used. */
extern void VdGetCurrentDisplayInformation(void *info);

static struct {
    uint32_t before[4];
    unsigned char info[0x58];
    uint32_t after[64];
} storage;

static unsigned read16(unsigned offset) {
    return ((unsigned)storage.info[offset] << 8) | storage.info[offset + 1];
}

int main(void) {
    FILE *log = fopen("game:\\xbox360-display.log", "wb");
    if (log) {
        memset(&storage, 0xa5, sizeof(storage));
        memset(storage.info, 0, sizeof(storage.info));
        fputs("Display query revision 1: no GPU state changes, no automatic exit\nBefore display information query\n", log);
        if (fflush(log) == 0) {
            VdGetCurrentDisplayInformation(storage.info);
            int guarded = 1;
            for (unsigned i = 0; i < 4; ++i) guarded &= storage.before[i] == 0xa5a5a5a5u;
            for (unsigned i = 0; i < 64; ++i) guarded &= storage.after[i] == 0xa5a5a5a5u;
            fprintf(log, "After display information query; guards %s\n", guarded ? "PASS" : "FAIL");
            fprintf(log, "Front buffer %u x %u; color=%u pixel=%u\n",
                    read16(0), read16(2), storage.info[4], storage.info[5]);
            fprintf(log, "Display %u x %u; actual width=%u\n",
                    read16(0x48), read16(0x4a), read16(0x56));
            fputs("Raw display information:", log);
            for (unsigned i = 0; i < sizeof(storage.info); ++i)
                fprintf(log, "%s%02x", i % 16 ? " " : "\n", storage.info[i]);
            fputs("\nQuery returned; presentation NOT tested\nParking; leave through Guide\n", log);
        }
        fclose(log);
    }
    for (;;) {
        int64_t interval = -10000000;
        KeDelayExecutionThread(0, 0, &interval);
    }
}
