#include <stdio.h>
#include "door_capability.h"
double door_calculate_score(const node_profile_t *node) {
    if (!node) return 0.0;
    double cap_norm = (node->available_capacity > 1000) ? 1.0 : (double)node->available_capacity / 1000.0;
    double part_norm = (node->participation_count > 100) ? 1.0 : (double)node->participation_count / 100.0;
    double score = (node->reputation * 0.30) +
                   (node->stability_score * 0.25) +
                   (node->reliability * 0.20) +
                   (cap_norm * 15.0) +
                   (part_norm * 10.0);
    return score;
}
int door_evaluate_capability(node_profile_t *node) {
    if (!node) return 0;
    double score = door_calculate_score(node);
    if (score >= DOOR_SCORE_THRESHOLD) {
        node->is_door = 1;
        node->flags.radio_receive  = 1;
        node->flags.radio_transmit = 1;
        node->flags.wave_amplify   = 1;
        node->flags.resource_relay = 1;
    } else {
        node->is_door = 0;
        node->flags.radio_receive  = 1;
        node->flags.radio_transmit = 0;
        node->flags.wave_amplify   = 0;
        node->flags.resource_relay = 0;
    }
    return node->is_door;
}
void door_apply_decay(node_profile_t *node, double decay_rate) {
    if (!node) return;
    node->reputation -= decay_rate;
    if (node->reputation < 0.0) node->reputation = 0.0;
    node->stability_score -= (decay_rate * 0.5);
    if (node->stability_score < 0.0) node->stability_score = 0.0;
    door_evaluate_capability(node);
}
