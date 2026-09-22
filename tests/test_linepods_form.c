#include "linepods_form.h"

#include <assert.h>
#include <string.h>

static void test_decodes_fields(void)
{
    const char *body = "ssid=Home+Wi-Fi&password=a%2Bb%26c&key=wrk-123";
    char value[32];
    assert(linepods_form_value(body, "ssid", value, sizeof(value)));
    assert(strcmp(value, "Home Wi-Fi") == 0);
    assert(linepods_form_value(body, "password", value, sizeof(value)));
    assert(strcmp(value, "a+b&c") == 0);
    assert(linepods_form_value(body, "key", value, sizeof(value)));
    assert(strcmp(value, "wrk-123") == 0);
}

static void test_rejects_bad_or_oversized_values(void)
{
    char value[5] = "old";
    assert(!linepods_form_value("ssid=abcdef", "ssid", value, sizeof(value)));
    assert(!linepods_form_value("ssid=bad%2", "ssid", value, sizeof(value)));
    assert(!linepods_form_value("ssid=bad%GG", "ssid", value, sizeof(value)));
    assert(!linepods_form_value("password=x", "ssid", value, sizeof(value)));
    assert(value[0] == '\0');
}

int main(void)
{
    test_decodes_fields();
    test_rejects_bad_or_oversized_values();
    return 0;
}

