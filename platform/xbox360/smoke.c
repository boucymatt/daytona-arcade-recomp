/* ROM-free boot probe. No game, graphics, input or audio is claimed here. */
#include <stdio.h>
#include <string.h>
#include <xecore/xboxkrnl_debug.h>
#include <xecore/xboxkrnl_threads.h>
#include "guest_le.h"

_Static_assert(sizeof(void *) == 4, "Xbox ABI needs 32-bit pointers");
#if __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
#error Xbox 360 requires a big-endian target
#endif

int main(void) {
    volatile uint32_t native_word = 0x12345678;
    const int pass = guest_le_selftest() &&
                    ((volatile const unsigned char *)&native_word)[0] == 0x12;
    const char *message = pass ? "Daytona Xbox 360 boot probe: endian PASS\n"
                               : "Daytona Xbox 360 boot probe: endian FAIL\n";
    DbgPrint("%s", message);
    FILE *log = fopen("game:\\xbox360-boot.log", "wb");
    if (log) {
        int written = fputs(message, log);
        int closed = fclose(log);
        if (written < 0 || closed != 0) DbgPrint("Daytona: marker write/close failed\n");
        else {
            char readback[96] = {0};
            log = fopen("game:\\xbox360-boot.log", "rb");
            if (log) {
                size_t count = fread(readback, 1, sizeof(readback) - 1, log);
                int read_error = ferror(log);
                int read_closed = fclose(log);
                if (!read_error && !read_closed && count == strlen(message) &&
                    strcmp(readback, message) == 0) {
                    log = fopen("game:\\xbox360-boot.log", "ab");
                    if (log) {
                        int appended = fputs("Filesystem write/read-back PASS\n", log);
                        int append_closed = fclose(log);
                        if (appended < 0 || append_closed != 0)
                            DbgPrint("Daytona: final marker append failed\n");
                    }
                } else DbgPrint("Daytona: marker read-back failed\n");
            } else DbgPrint("Daytona: cannot reopen marker\n");
        }
    } else {
        DbgPrint("Daytona: cannot open game-relative boot marker\n");
    }
    int64_t interval = -30000000; /* relative 100 ns units: three seconds */
    KeDelayExecutionThread(0, 0, &interval);
    return pass ? 0 : 1; /* OpenXeChain newlib exit relaunches dashboard. */
}
