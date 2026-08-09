#ifndef WAVE_TEMPORAL_MANIFOLD_H
#define WAVE_TEMPORAL_MANIFOLD_H

#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <string.h>

#define TEMPORAL_LAYERS_COUNT 116

typedef struct {
    double amplitude;
    double frequency;
    double phase;
} temporal_layer_t;

typedef struct {
    temporal_layer_t layers[TEMPORAL_LAYERS_COUNT];
} space_filling_node_t;

static inline void init_space_filling_node(space_filling_node_t *node, double base_amp, double base_freq) {
    for (int i = 0; i < TEMPORAL_LAYERS_COUNT; i++) {
        node->layers[i].amplitude = base_amp * (1.0 + 0.1 * sin(i * 0.7));
        node->layers[i].frequency = base_freq * (1.0 + 0.05 * i);
        node->layers[i].phase = (double)i * M_PI / 58.0;
    }
}

static inline double surface_fluctuation(space_filling_node_t *node, double t) {
    double s = 0.0;
    for (int i = 0; i < TEMPORAL_LAYERS_COUNT; i++)
        s += node->layers[i].amplitude * sin(node->layers[i].frequency * t + node->layers[i].phase);
    return s;
}

static inline double phase_correction(space_filling_node_t *node, double t, double expected_phase) {
    double complex z = 0.0;
    for (int i = 0; i < TEMPORAL_LAYERS_COUNT; i++) {
        double theta = node->layers[i].frequency * t + node->layers[i].phase;
        z += node->layers[i].amplitude * cexp(I * theta);
    }
    double surface_phase = carg(z);
    double diff = expected_phase - surface_phase;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    return diff;
}

#endif
