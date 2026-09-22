#pragma once

#include <stddef.h>

/* Returns a bounded geometric capacity that can hold required bytes, or zero
 * when the configured limit would be exceeded. */
size_t linepods_response_next_capacity(size_t current, size_t required,
                                     size_t limit);
