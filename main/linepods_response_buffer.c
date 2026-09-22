#include "linepods_response_buffer.h"

size_t linepods_response_next_capacity(size_t current, size_t required,
                                     size_t limit)
{
    if (required == 0 || limit == 0 || required > limit) return 0;
    if (current >= required) return current;

    size_t next = current > 0 ? current : 4096;
    while (next < required) {
        if (next > limit / 2) {
            next = limit;
            break;
        }
        next *= 2;
    }
    return next >= required ? next : 0;
}
