#include "linepods_sync_policy.h"

linepods_sync_decision_t linepods_sync_policy_decide(
    bool has_cache, bool deep_sleep_wakeup, bool system_time_valid,
    int64_t now_unix, int64_t last_success_unix, uint32_t ttl_seconds)
{
    if (!has_cache) return LINEPODS_SYNC_REQUIRED_NO_CACHE;

    bool age_known = system_time_valid && last_success_unix > 0
                  && now_unix >= last_success_unix;
    if (age_known) {
        uint64_t age = (uint64_t)(now_unix - last_success_unix);
        return age >= ttl_seconds ? LINEPODS_SYNC_REQUIRED_STALE_CACHE
                                  : LINEPODS_SYNC_SKIP_FRESH_CACHE;
    }

    return deep_sleep_wakeup ? LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE
                             : LINEPODS_SYNC_REQUIRED_COLD_BOOT;
}

bool linepods_sync_policy_should_sync(linepods_sync_decision_t decision)
{
    return decision == LINEPODS_SYNC_REQUIRED_NO_CACHE
        || decision == LINEPODS_SYNC_REQUIRED_STALE_CACHE
        || decision == LINEPODS_SYNC_REQUIRED_COLD_BOOT;
}

const char *linepods_sync_policy_decision_name(linepods_sync_decision_t decision)
{
    switch (decision) {
    case LINEPODS_SYNC_REQUIRED_NO_CACHE:
        return "no-cache";
    case LINEPODS_SYNC_REQUIRED_STALE_CACHE:
        return "stale-cache";
    case LINEPODS_SYNC_REQUIRED_COLD_BOOT:
        return "cold-boot-unknown-age";
    case LINEPODS_SYNC_SKIP_FRESH_CACHE:
        return "fresh-cache";
    case LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE:
        return "deep-wake-unknown-age";
    default:
        return "unknown";
    }
}
