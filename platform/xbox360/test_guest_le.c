#include "guest_le.h"
#include <stdio.h>

int main(void) {
    if (!guest_le_selftest()) return 1;
    unsigned char b[9];
    for (unsigned offset = 0; offset < 4; ++offset) {
        for (uint32_t v = 0; v < 65536; ++v) {
            uint32_t word = v | ((65535u - v) << 16);
            guest_store_le32(b + offset, word);
            if (guest_le32(b + offset) != word || guest_le16(b + offset) != v ||
                guest_le16(b + offset + 2) != 65535u - v) return 2;
        }
    }
    puts("262144 guest LE offset/value cases passed (host test only)");
    return 0;
}
