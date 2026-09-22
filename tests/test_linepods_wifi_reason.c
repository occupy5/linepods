#include "linepods_wifi_reason.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    assert(linepods_wifi_classify_reason(201) == LINEPODS_WIFI_FAILURE_NOT_FOUND);
    assert(linepods_wifi_classify_reason(211) == LINEPODS_WIFI_FAILURE_NOT_FOUND);
    assert(linepods_wifi_classify_reason(202) == LINEPODS_WIFI_FAILURE_AUTHENTICATION);
    assert(linepods_wifi_classify_reason(204) == LINEPODS_WIFI_FAILURE_AUTHENTICATION);
    assert(linepods_wifi_classify_reason(203) == LINEPODS_WIFI_FAILURE_COMPATIBILITY);
    assert(linepods_wifi_classify_reason(200) == LINEPODS_WIFI_FAILURE_SIGNAL);
    assert(linepods_wifi_classify_reason(1) == LINEPODS_WIFI_FAILURE_UNKNOWN);

    char message[96];
    linepods_wifi_format_failure(202, message, sizeof(message));
    assert(strstr(message, "202") != NULL);
    assert(strstr(message, "密码") != NULL);

    char short_message[8];
    linepods_wifi_format_failure(201, short_message, sizeof(short_message));
    assert(short_message[sizeof(short_message) - 1] == '\0');
    return 0;
}
