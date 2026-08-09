#include <stdio.h>
#include <string.h>
#include "wave_generator.h"
void wave_generator_init(wave_generator_t *gen, uint64_t id) {
    if (!gen) return;
    gen->generator_id = id;
    gen->base_rhythm = (double)id;
    gen->current_phase = 0.0;
    gen->current_amplitude = 1.0;
}
wd_trace_t wave_generator_emit(wave_generator_t *gen, const char *content_prefix, uint64_t seq) {
    wd_trace_t trace;
    memset(&trace, 0, sizeof(trace));
    trace.rhythm = gen->generator_id;
    trace.phase = gen->current_phase;
    trace.type = 1;
    snprintf(trace.content, sizeof(trace.content), "%s_seq%" PRIu64 "_amp%.1f",
             content_prefix, seq, gen->current_amplitude);
    trace.size = strlen(trace.content);
    return trace;
}
