#include "linepods_power_policy.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

static const linepods_power_policy_config_t CONFIG = {
    .dim_after_ms = 10000,
    .screen_off_after_ms = 30000,
    .radio_off_after_ms = 60000,
    .deep_sleep_after_ms = 300000,
};

static linepods_power_policy_t make_policy(void)
{
    linepods_power_policy_t policy;
    linepods_power_policy_init(&policy, &CONFIG, 0);
    return policy;
}

static void test_phases_advance_at_thresholds(void)
{
    linepods_power_policy_t policy = make_policy();

    assert(linepods_power_policy_update(&policy, 9999, true, true, true)
           == LINEPODS_POWER_ACTIVE);
    /* The boundary itself is inclusive: at exactly dim_after_ms the phase flips. */
    assert(linepods_power_policy_update(&policy, 10000, true, true, true)
           == LINEPODS_POWER_DIM);
    assert(linepods_power_policy_update(&policy, 29999, true, true, true)
           == LINEPODS_POWER_DIM);
    assert(linepods_power_policy_update(&policy, 30000, true, true, true)
           == LINEPODS_POWER_SCREEN_OFF);
    assert(linepods_power_policy_update(&policy, 59999, true, true, true)
           == LINEPODS_POWER_SCREEN_OFF);
    assert(linepods_power_policy_update(&policy, 60000, true, true, true)
           == LINEPODS_POWER_RADIO_OFF);
    /* A later phase implies all earlier savings and never expires upward. */
    assert(linepods_power_policy_update(&policy, 900000, true, true, true)
           == LINEPODS_POWER_RADIO_OFF);
}

static void test_activity_resets_every_phase(void)
{
    linepods_power_policy_t policy = make_policy();
    assert(linepods_power_policy_update(&policy, 60000, true, true, true)
           == LINEPODS_POWER_RADIO_OFF);

    linepods_power_policy_record_activity(&policy, 60000);
    assert(policy.state == LINEPODS_POWER_ACTIVE);
    assert(linepods_power_policy_update(&policy, 60999, true, true, true)
           == LINEPODS_POWER_ACTIVE);
    assert(linepods_power_policy_update(&policy, 70000, true, true, true)
           == LINEPODS_POWER_DIM);
}

static void test_gate_flags_hold_later_phases(void)
{
    linepods_power_policy_t policy = make_policy();

    /* Screen-off blocked (e.g. provisioning): stop at DIM, never blank. */
    assert(linepods_power_policy_update(&policy, 60000, true, false, true)
           == LINEPODS_POWER_DIM);

    /* Dim also blocked: stay fully active so the user can keep working. */
    assert(linepods_power_policy_update(&policy, 60000, false, false, true)
           == LINEPODS_POWER_ACTIVE);

    /* Screen may blank but the radio must stay up (in-flight fetch). */
    assert(linepods_power_policy_update(&policy, 60000, true, true, false)
           == LINEPODS_POWER_SCREEN_OFF);

    /* Gate lifted: the ladder catches up on the next evaluation. */
    assert(linepods_power_policy_update(&policy, 60000, true, true, true)
           == LINEPODS_POWER_RADIO_OFF);
}

static void test_time_moving_backwards_is_clamped(void)
{
    linepods_power_policy_t policy = make_policy();
    /* A non-monotonic sample must not restart the ladder or flip phases. */
    assert(linepods_power_policy_update(&policy, 5000, true, true, true)
           == LINEPODS_POWER_ACTIVE);
    linepods_power_policy_record_activity(&policy, 60000);
    assert(linepods_power_policy_update(&policy, 100, true, true, true)
           == LINEPODS_POWER_ACTIVE);
}

static void test_deep_sleep_is_gated_and_uses_raw_elapsed(void)
{
    linepods_power_policy_t policy = make_policy();

    assert(!linepods_power_policy_deep_sleep_due(&policy, 299999, true));
    assert(linepods_power_policy_deep_sleep_due(&policy, 300000, true));
    /* Deep sleep is terminal, so it stays explicitly gated. */
    assert(!linepods_power_policy_deep_sleep_due(&policy, 300000, false));

    linepods_power_policy_record_activity(&policy, 300000);
    assert(!linepods_power_policy_deep_sleep_due(&policy, 300000, true));
    assert(linepods_power_policy_deep_sleep_due(&policy, 600000, true));
}

static void test_disabled_deep_sleep_never_fires(void)
{
    linepods_power_policy_t policy;
    linepods_power_policy_config_t config = CONFIG;
    config.deep_sleep_after_ms = 0;
    linepods_power_policy_init(&policy, &config, 0);

    assert(!linepods_power_policy_deep_sleep_due(&policy, 1000000, true));
}

static void test_null_policy_is_safe(void)
{
    assert(linepods_power_policy_update(NULL, 60000, true, true, true)
           == LINEPODS_POWER_ACTIVE);
    assert(!linepods_power_policy_deep_sleep_due(NULL, 60000, true));
    linepods_power_policy_record_activity(NULL, 60000);
}

int main(void)
{
    test_phases_advance_at_thresholds();
    test_activity_resets_every_phase();
    test_gate_flags_hold_later_phases();
    test_time_moving_backwards_is_clamped();
    test_deep_sleep_is_gated_and_uses_raw_elapsed();
    test_disabled_deep_sleep_never_fires();
    test_null_policy_is_safe();
    return 0;
}
