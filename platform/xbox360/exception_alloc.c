/* Single-threaded exception bring-up only. The pinned newlib allocator
   returns page-backed blocks after a 16-byte header. Check that assumption
   at runtime; never fabricate an aligned pointer that free cannot consume. */
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

void *aligned_alloc(size_t alignment, size_t size) {
    if (!alignment || (alignment & (alignment - 1)) || size % alignment) {
        errno = EINVAL;
        return NULL;
    }
    if (alignment > 16) { errno = ENOMEM; return NULL; }
    void *result = malloc(size ? size : alignment);
    if (result && (uintptr_t)result % alignment) {
        free(result);
        errno = ENOMEM;
        return NULL;
    }
    return result;
}
