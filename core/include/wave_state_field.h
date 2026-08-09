#ifndef WAVE_STATE_FIELD_H
#define WAVE_STATE_FIELD_H

typedef struct {
    double residual;
    double coherence_raw;
    double coherence_memory;
    double dimensional_state;
    double dimensional_velocity;
} wave_state_t;

#endif
