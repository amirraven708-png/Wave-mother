#include <stdio.h>
#include <math.h>
#include "wave_behavior.h"

void behavior_init(behavior_engine_t *be) {
    be->state = BEHAVIOR_REACTIVE;       // Start conservatively
    be->local_load = 0.0;
    be->network_need_level = 0.0;
    be->available_capacity = 1.0;
    be->reputation = 70.0;
    be->proactive_enter_threshold = 0.4;  // Enter proactive when need > 0.4
    be->reactive_enter_threshold  = 0.15; // Return to reactive when need < 0.15
    be->load_limit = 0.85;               // If load > 85%, stay reactive
    be->heartbeat_phase = 0.0;
    be->heartbeat_freq = 0.5;            // 0.5 Hz base rhythm
}

void behavior_update(behavior_engine_t *be,
                     double local_load,
                     double network_need_level,
                     double available_capacity,
                     double reputation,
                     double dt) {
    // Update metrics
    be->local_load = local_load;
    be->network_need_level = network_need_level;
    be->available_capacity = available_capacity;
    be->reputation = reputation;

    // Always advance heartbeat
    be->heartbeat_phase += be->heartbeat_freq * 360.0 * dt;
    if (be->heartbeat_phase >= 360.0) be->heartbeat_phase -= 360.0;

    // Decision logic
    if (be->state == BEHAVIOR_REACTIVE) {
        // Enter PROACTIVE if:
        // 1. Network needs help (high need level)
        // 2. We have capacity to give
        // 3. We are not overloaded
        if (be->network_need_level >= be->proactive_enter_threshold &&
            be->available_capacity > 0.2 &&
            be->local_load < be->load_limit) {
            be->state = BEHAVIOR_PROACTIVE;
            printf("[Behavior] Switching to PROACTIVE – network needs waves.\n");
        }
    } else { // BEHAVIOR_PROACTIVE
        // Return to REACTIVE if:
        // 1. Network is calm (low need level)
        // 2. OR we are overloaded
        if (be->network_need_level <= be->reactive_enter_threshold ||
            be->local_load >= be->load_limit) {
            be->state = BEHAVIOR_REACTIVE;
            printf("[Behavior] Switching to REACTIVE – conserving resources.\n");
        }
    }
}

int behavior_is_proactive(behavior_engine_t *be) {
    return be->state == BEHAVIOR_PROACTIVE;
}

void behavior_force_proactive(behavior_engine_t *be) {
    be->state = BEHAVIOR_PROACTIVE;
    printf("[Behavior] Forced PROACTIVE.\n");
}

void behavior_force_reactive(behavior_engine_t *be) {
    be->state = BEHAVIOR_REACTIVE;
    printf("[Behavior] Forced REACTIVE.\n");
}

const char* behavior_state_name(behavior_state_t s) {
    return (s == BEHAVIOR_PROACTIVE) ? "PROACTIVE (Awake)" : "REACTIVE (Sleep)";
}
