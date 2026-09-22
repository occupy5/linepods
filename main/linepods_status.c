#include "linepods_status.h"

#include <stdio.h>

/* Treat dates before 2024 as an unset RTC. ESP-IDF starts near the Unix epoch
 * until SNTP has set the system clock. */
#define LINEPODS_MIN_VALID_TIME ((time_t)1704067200)

bool linepods_status_time_valid(time_t timestamp)
{
    return timestamp >= LINEPODS_MIN_VALID_TIME;
}

bool linepods_status_format_time(time_t timestamp, char destination[6])
{
    if (!destination) return false;
    if (!linepods_status_time_valid(timestamp)) {
        snprintf(destination, 6, "--:--");
        return false;
    }

    struct tm local = {0};
    if (!localtime_r(&timestamp, &local)) {
        snprintf(destination, 6, "--:--");
        return false;
    }
    snprintf(destination, 6, "%02d:%02d", local.tm_hour, local.tm_min);
    return true;
}

unsigned linepods_status_battery_segments(int percent)
{
    if (percent <= 0 || percent > 100) return 0;
    return (unsigned)(percent + 24) / 25;
}
