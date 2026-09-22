#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Ordered idle phases. A later phase includes the savings of all earlier
 * phases; LINEPODS_POWER_RADIO_OFF keeps the backlight off and also stops the
 * long-lived Wi-Fi service until the next explicit network action. */
typedef enum {
    LINEPODS_POWER_ACTIVE = 0,
    LINEPODS_POWER_DIM,
    LINEPODS_POWER_SCREEN_OFF,
    LINEPODS_POWER_RADIO_OFF,
} linepods_power_state_t;

typedef struct {
    uint32_t dim_after_ms;
    uint32_t screen_off_after_ms;
    uint32_t radio_off_after_ms;
    uint32_t deep_sleep_after_ms;
} linepods_power_policy_config_t;

typedef struct {
    linepods_power_policy_config_t config;
    uint64_t last_activity_ms;
    linepods_power_state_t state;
} linepods_power_policy_t;

/* Initialize a policy with an already monotonic time base. The caller owns the
 * object; the module performs no allocation or blocking work. */
void linepods_power_policy_init(linepods_power_policy_t *policy,
                              const linepods_power_policy_config_t *config,
                              uint64_t now_ms);

/* Record any user input as activity and return the policy to ACTIVE. */
void linepods_power_policy_record_activity(linepods_power_policy_t *policy,
                                         uint64_t now_ms);

/* Return the highest currently permitted idle phase. The phases are cumulative,
 * so a phase is only selected once every earlier gate is open as well; the
 * caller can use the gate flags to hold a phase back, e.g. keep the radio up
 * while an in-flight transfer or the setup access point still needs it. */
linepods_power_state_t linepods_power_policy_update(
    linepods_power_policy_t *policy, uint64_t now_ms, bool allow_dim,
    bool allow_screen_off, bool allow_radio_off);

/* Deep sleep is terminal and restarts the application, so it is separate from
 * the reversible display/radio phases and is always explicitly gated. */
bool linepods_power_policy_deep_sleep_due(const linepods_power_policy_t *policy,
                                        uint64_t now_ms, bool allow_deep_sleep);
