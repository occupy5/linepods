#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LINEPODS_SYNC_REQUIRED_NO_CACHE = 0,
    LINEPODS_SYNC_REQUIRED_STALE_CACHE,
    LINEPODS_SYNC_REQUIRED_COLD_BOOT,
    LINEPODS_SYNC_SKIP_FRESH_CACHE,
    LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE,
} linepods_sync_decision_t;

/* Decide whether boot should start a background incremental sync. Manual sync
 * bypasses this policy. When the cache age cannot be established, a cold boot
 * refreshes it while a button wake favors immediate offline reading. */
linepods_sync_decision_t linepods_sync_policy_decide(
    bool has_cache, bool deep_sleep_wakeup, bool system_time_valid,
    int64_t now_unix, int64_t last_success_unix, uint32_t ttl_seconds);

bool linepods_sync_policy_should_sync(linepods_sync_decision_t decision);
const char *linepods_sync_policy_decision_name(linepods_sync_decision_t decision);
