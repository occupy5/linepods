#include "linepods_sync_policy.h"

#include <assert.h>
#include <string.h>

#define TTL_SECONDS (12U * 60U * 60U)

static void test_missing_cache_always_syncs(void)
{
    linepods_sync_decision_t decision = linepods_sync_policy_decide(
        false, true, false, 0, 0, TTL_SECONDS);
    assert(decision == LINEPODS_SYNC_REQUIRED_NO_CACHE);
    assert(linepods_sync_policy_should_sync(decision));
}

static void test_fresh_cache_skips_network_on_any_boot(void)
{
    linepods_sync_decision_t cold = linepods_sync_policy_decide(
        true, false, true, 100000, 100000 - TTL_SECONDS + 1, TTL_SECONDS);
    linepods_sync_decision_t wake = linepods_sync_policy_decide(
        true, true, true, 100000, 100000 - TTL_SECONDS + 1, TTL_SECONDS);
    assert(cold == LINEPODS_SYNC_SKIP_FRESH_CACHE);
    assert(wake == LINEPODS_SYNC_SKIP_FRESH_CACHE);
    assert(!linepods_sync_policy_should_sync(cold));
    assert(!linepods_sync_policy_should_sync(wake));
}

static void test_stale_boundary_is_inclusive(void)
{
    linepods_sync_decision_t decision = linepods_sync_policy_decide(
        true, true, true, 100000, 100000 - TTL_SECONDS, TTL_SECONDS);
    assert(decision == LINEPODS_SYNC_REQUIRED_STALE_CACHE);
    assert(linepods_sync_policy_should_sync(decision));
}

static void test_unknown_age_prefers_offline_deep_wake(void)
{
    linepods_sync_decision_t missing_timestamp = linepods_sync_policy_decide(
        true, true, true, 100000, 0, TTL_SECONDS);
    linepods_sync_decision_t invalid_clock = linepods_sync_policy_decide(
        true, true, false, 0, 90000, TTL_SECONDS);
    linepods_sync_decision_t clock_rollback = linepods_sync_policy_decide(
        true, true, true, 80000, 90000, TTL_SECONDS);
    assert(missing_timestamp == LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE);
    assert(invalid_clock == LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE);
    assert(clock_rollback == LINEPODS_SYNC_SKIP_DEEP_WAKE_UNKNOWN_AGE);
}

static void test_unknown_age_refreshes_on_cold_boot(void)
{
    linepods_sync_decision_t decision = linepods_sync_policy_decide(
        true, false, false, 0, 0, TTL_SECONDS);
    assert(decision == LINEPODS_SYNC_REQUIRED_COLD_BOOT);
    assert(linepods_sync_policy_should_sync(decision));
    assert(strcmp(linepods_sync_policy_decision_name(decision),
                  "cold-boot-unknown-age") == 0);
}

int main(void)
{
    test_missing_cache_always_syncs();
    test_fresh_cache_skips_network_on_any_boot();
    test_stale_boundary_is_inclusive();
    test_unknown_age_prefers_offline_deep_wake();
    test_unknown_age_refreshes_on_cold_boot();
    return 0;
}
