#include "linepods_status.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char text[6];
    assert(!linepods_status_time_valid(0));
    assert(!linepods_status_format_time(0, text));
    assert(strcmp(text, "--:--") == 0);

    assert(setenv("TZ", "UTC0", 1) == 0);
    tzset();
    assert(linepods_status_time_valid(1704164640));
    assert(linepods_status_format_time(1704164640, text));
    assert(strcmp(text, "03:04") == 0);

    assert(linepods_status_battery_segments(-1) == 0);
    assert(linepods_status_battery_segments(0) == 0);
    assert(linepods_status_battery_segments(1) == 1);
    assert(linepods_status_battery_segments(25) == 1);
    assert(linepods_status_battery_segments(26) == 2);
    assert(linepods_status_battery_segments(50) == 2);
    assert(linepods_status_battery_segments(51) == 3);
    assert(linepods_status_battery_segments(75) == 3);
    assert(linepods_status_battery_segments(76) == 4);
    assert(linepods_status_battery_segments(100) == 4);
    assert(linepods_status_battery_segments(101) == 0);
    return 0;
}
