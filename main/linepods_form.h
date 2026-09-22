#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Finds and URL-decodes one application/x-www-form-urlencoded field. */
bool linepods_form_value(const char *body, const char *name,
                       char *destination, size_t capacity);

