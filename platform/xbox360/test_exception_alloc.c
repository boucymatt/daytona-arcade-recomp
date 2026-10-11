#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    for (size_t alignment = 1; alignment <= 16; alignment *= 2) {
        for (size_t count = 0; count < 64; ++count) {
            size_t size = count * alignment;
            void *p = aligned_alloc(alignment, size);
            assert(p && (uintptr_t)p % alignment == 0);
            memset(p, 0x5a, size);
            free(p);
        }
    }
    errno = 0;
    assert(!aligned_alloc(3, 12) && errno == EINVAL);
    assert(!aligned_alloc(16, 17) && errno == EINVAL);
    assert(!aligned_alloc(32, 64) && errno == ENOMEM);
    return 0;
}
