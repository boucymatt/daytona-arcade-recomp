/* Explicit guest byte order. No unaligned host loads or aliasing assumptions. */
#ifndef DAYTONA_XBOX360_GUEST_LE_H
#define DAYTONA_XBOX360_GUEST_LE_H
#include <stdint.h>
static inline uint16_t guest_le16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t guest_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void guest_store_le32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static inline int guest_le_selftest(void) {
    unsigned char bytes[6] = {0xa5, 0x78, 0x56, 0x34, 0x12, 0x5a};
    if (guest_le16(bytes + 1) != 0x5678 || guest_le32(bytes + 1) != 0x12345678) return 0;
    guest_store_le32(bytes + 1, 0xfedcba98u);
    return bytes[0] == 0xa5 && bytes[5] == 0x5a && bytes[1] == 0x98 &&
           bytes[2] == 0xba && bytes[3] == 0xdc && bytes[4] == 0xfe;
}
#endif
