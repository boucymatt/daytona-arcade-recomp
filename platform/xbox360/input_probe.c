/* Separate optional probe: four XAM gamepad slots, no game/graphics/audio. */
#include <stdio.h>
#include <xecore/xboxkrnl_threads.h>
#include "controller.h"

/* xecorelib supplies the ordinal-401 import, but not its C declaration.
   Xenia's public xam_input.cc verifies (user, flags, 16-byte state pointer).
   uint32_t storage guarantees the required alignment; decoding is explicit. */
extern uint32_t XamInputGetState(uint32_t user, uint32_t flags, void *state);
_Static_assert(sizeof(void *) == 4, "Xbox ABI requires 32-bit pointers");
#if __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
#error Xbox 360 requires big-endian PowerPC
#endif

int main(void) {
    FILE *log = fopen("game:\\xbox360-input.log", "wb");
    if (!log) return 1;
    uint32_t previous_packet[4] = {0}, previous_status[4] = {0};
    fputs("Input probe: eight seconds; move sticks and triggers\n", log);
    for (unsigned frame = 0; frame < 400; ++frame) {
        for (unsigned user = 0; user < 4; ++user) {
            uint32_t raw[4] = {0};
            uint32_t status = XamInputGetState(user, 1, raw);
            DaytonaPad pad = decode_pad((const unsigned char *)raw);
            if (frame == 0 || status != previous_status[user] ||
                (status == 0 && pad.packet != previous_packet[user])) {
                fprintf(log, "slot=%u status=%lu packet=%lu buttons=%04x steer=%u gas=%u brake=%u\n",
                        user, (unsigned long)status, (unsigned long)pad.packet,
                        (unsigned)pad.buttons, status ? 128u : (unsigned)pad_steer(&pad),
                        status ? 32u : (unsigned)pad_pedal(pad.right_trigger),
                        status ? 32u : (unsigned)pad_pedal(pad.left_trigger));
                previous_status[user] = status;
                previous_packet[user] = pad.packet;
            }
        }
        if (frame % 50 == 0 && fflush(log) != 0) { fclose(log); return 2; }
        int64_t interval = -200000; /* 20 ms */
        KeDelayExecutionThread(0, 0, &interval);
    }
    return fclose(log) == 0 ? 0 : 3;
}
