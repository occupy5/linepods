#include "linepods_power_policy.h"

static uint64_t elapsed_ms(uint64_t now_ms, uint64_t since_ms)
{
    return now_ms >= since_ms ? now_ms - since_ms : 0;
}

void linepods_power_policy_init(linepods_power_policy_t *policy,
                              const linepods_power_policy_config_t *config,
                              uint64_t now_ms)
{
    if (!policy) return;

    policy->config = config ? *config : (linepods_power_policy_config_t) {0};
    policy->last_activity_ms = now_ms;
    policy->state = LINEPODS_POWER_ACTIVE;
}

void linepods_power_policy_record_activity(linepods_power_policy_t *policy,
                                         uint64_t now_ms)
{
    if (!policy) return;

    policy->last_activity_ms = now_ms;
    policy->state = LINEPODS_POWER_ACTIVE;
}

linepods_power_state_t linepods_power_policy_update(
    linepods_power_policy_t *policy, uint64_t now_ms, bool allow_dim,
    bool allow_screen_off, bool allow_radio_off)
{
    if (!policy) return LINEPODS_POWER_ACTIVE;

    uint64_t elapsed = elapsed_ms(now_ms, policy->last_activity_ms);

    /* The phases are cumulative: a later phase is only reachable once every
     * earlier gate is open and its own threshold has elapsed. Without this,
     * blocking screen-off (e.g. while provisioning) could still blank the panel
     * by jumping straight to the radio-off phase. */
    bool dim = allow_dim && elapsed >= policy->config.dim_after_ms;
    bool screen_off = dim && allow_screen_off
                      && elapsed >= policy->config.screen_off_after_ms;
    bool radio_off = screen_off && allow_radio_off
                     && elapsed >= policy->config.radio_off_after_ms;

    linepods_power_state_t target = radio_off ? LINEPODS_POWER_RADIO_OFF
                                  : screen_off ? LINEPODS_POWER_SCREEN_OFF
                                  : dim ? LINEPODS_POWER_DIM
                                  : LINEPODS_POWER_ACTIVE;

    policy->state = target;
    return target;
}

bool linepods_power_policy_deep_sleep_due(const linepods_power_policy_t *policy,
                                        uint64_t now_ms,
                                        bool allow_deep_sleep)
{
    if (!policy || !allow_deep_sleep || policy->config.deep_sleep_after_ms == 0) {
        return false;
    }

    return elapsed_ms(now_ms, policy->last_activity_ms)
           >= policy->config.deep_sleep_after_ms;
}
