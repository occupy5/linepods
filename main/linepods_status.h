#pragma once

#include <stdbool.h>
#include <time.h>

#define LINEPODS_BATTERY_SEGMENT_COUNT 4

bool linepods_status_time_valid(time_t timestamp);
bool linepods_status_format_time(time_t timestamp, char destination[6]);
unsigned linepods_status_battery_segments(int percent);
