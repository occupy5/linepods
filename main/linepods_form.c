#include "linepods_form.h"

#include <ctype.h>
#include <string.h>

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    value = (char)tolower((unsigned char)value);
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static bool url_decode(char *destination, size_t capacity,
                       const char *source, size_t source_length)
{
    if (!destination || capacity == 0 || !source) return false;
    size_t written = 0;
    for (size_t i = 0; i < source_length; i++) {
        unsigned char value = (unsigned char)source[i];
        if (value == '+') {
            value = ' ';
        } else if (value == '%' && i + 2 < source_length) {
            int high = hex_value(source[i + 1]);
            int low = hex_value(source[i + 2]);
            if (high < 0 || low < 0) return false;
            value = (unsigned char)((high << 4) | low);
            i += 2;
        } else if (value == '%') {
            return false;
        }
        if (value == 0 || written + 1 >= capacity) return false;
        destination[written++] = (char)value;
    }
    destination[written] = '\0';
    return true;
}

bool linepods_form_value(const char *body, const char *name,
                       char *destination, size_t capacity)
{
    if (!body || !name || !destination || capacity == 0) return false;
    destination[0] = '\0';
    size_t name_length = strlen(name);
    const char *cursor = body;
    while (*cursor) {
        const char *end = strchr(cursor, '&');
        if (!end) end = cursor + strlen(cursor);
        const char *equals = memchr(cursor, '=', (size_t)(end - cursor));
        if (equals && (size_t)(equals - cursor) == name_length
            && memcmp(cursor, name, name_length) == 0) {
            bool decoded = url_decode(destination, capacity, equals + 1,
                                      (size_t)(end - equals - 1));
            if (!decoded) destination[0] = '\0';
            return decoded;
        }
        cursor = *end ? end + 1 : end;
    }
    return false;
}
