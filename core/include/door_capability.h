#ifndef DOOR_CAPABILITY_H
#define DOOR_CAPABILITY_H
#include <stdint.h>
#define DOOR_SCORE_THRESHOLD 65.0
typedef struct {
    uint8_t radio_receive  : 1;
    uint8_t radio_transmit : 1;
    uint8_t wave_amplify   : 1;
    uint8_t resource_relay : 1;
} door_flags_t;
typedef struct {
    uint64_t     node_id;
    double       reputation;
    double       stability_score;
    uint64_t     participation_count;
    uint64_t     available_capacity;
    double       reliability;
    door_flags_t flags;
    int          is_door;
} node_profile_t;
double door_calculate_score(const node_profile_t *node);
int    door_evaluate_capability(node_profile_t *node);
void   door_apply_decay(node_profile_t *node, double decay_rate);
#endif
