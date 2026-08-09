#ifndef WAVE_BEHAVIOR_H
#define WAVE_BEHAVIOR_H

#include <stdint.h>

typedef enum {
    BEHAVIOR_REACTIVE,      // "Sleep" – listen only, minimal heartbeat
    BEHAVIOR_PROACTIVE      // "Awake" – full wave generation & response
} behavior_state_t;

typedef struct {
    behavior_state_t state;

    // Metrics that influence the decision
    double   local_load;          // 0.0 (idle) – 1.0 (overloaded)
    double   network_need_level;  // Smoothed average of incoming Need Signals
    double   available_capacity;  // 0.0 – 1.0 normalized
    double   reputation;          // 0.0 – 100.0

    // Hysteresis thresholds
    double   proactive_enter_threshold;   // network_need_level must exceed this to become proactive
    double   reactive_enter_threshold;    // network_need_level must drop below this to become reactive
    double   load_limit;                  // if local_load > this, stay reactive regardless

    // Heartbeat (preserved in both states)
    double   heartbeat_phase;
    double   heartbeat_freq;
} behavior_engine_t;

void behavior_init(behavior_engine_t *be);
void behavior_update(behavior_engine_t *be,
                     double local_load,
                     double network_need_level,
                     double available_capacity,
                     double reputation,
                     double dt);

// Decision helpers
int  behavior_is_proactive(behavior_engine_t *be);
void behavior_force_proactive(behavior_engine_t *be);
void behavior_force_reactive(behavior_engine_t *be);

const char* behavior_state_name(behavior_state_t s);
#endif
