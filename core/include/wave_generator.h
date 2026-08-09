#ifndef WAVE_GENERATOR_H
#define WAVE_GENERATOR_H
#include <stdint.h>
#include "wave_trace.h"
typedef struct {
    uint64_t generator_id;
    double   base_rhythm;
    double   current_phase;
    double   current_amplitude;
} wave_generator_t;
void wave_generator_init(wave_generator_t *gen, uint64_t id);
wd_trace_t wave_generator_emit(wave_generator_t *gen, const char *content_prefix, uint64_t seq);
#endif
