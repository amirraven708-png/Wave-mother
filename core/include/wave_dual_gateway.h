#ifndef WAVE_DUAL_GATEWAY_H
#define WAVE_DUAL_GATEWAY_H

#include "wave_temporal_manifold.h"

#define WEST_RING_TOLERANCE 1e-6

typedef struct {
    double base_frequency;
    double current_macro_time;
    double accumulated_pulse_energy;
    space_filling_node_t temporal_node;
} eastern_gateway_t;

typedef struct {
    double target_coherence;
    double last_correction_vector;
    int unity_state_achieved;
} western_ring_t;

static inline void init_east(eastern_gateway_t *east, double base_freq) {
    east->base_frequency = base_freq;
    east->current_macro_time = 0.0;
    east->accumulated_pulse_energy = 0.0;
    init_space_filling_node(&east->temporal_node, 1.0, base_freq);
}

static inline double step_east(eastern_gateway_t *east, double dt) {
    east->current_macro_time += dt;
    double sig = surface_fluctuation(&east->temporal_node, east->current_macro_time);
    east->accumulated_pulse_energy += fabs(sig) * dt;
    return sig;
}

static inline double process_west(western_ring_t *west, eastern_gateway_t *east, double expected_phase) {
    double diff = phase_correction(&east->temporal_node, east->current_macro_time, expected_phase);
    west->last_correction_vector = diff;
    west->unity_state_achieved = (fabs(diff) < WEST_RING_TOLERANCE) ? 1 : 0;
    return diff;
}

#endif
