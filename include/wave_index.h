#ifndef WAVE_INDEX_H
#define WAVE_INDEX_H
#include <stdint.h>
#include <stddef.h>
#include "wave_trace.h"
typedef struct {
    size_t count;
    size_t capacity;
    size_t *trace_indices;
} rhythm_bucket_t;
typedef struct {
    size_t num_buckets;
    size_t num_traces;
    rhythm_bucket_t *buckets;
    const wd_trace_t *traces;
} wave_index_t;
void wave_index_build(wave_index_t *idx, const wd_trace_t *traces, size_t count, int num_buckets);
int wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t rhythm_mask,
                      uint64_t phase_min, uint64_t phase_max,
                      wd_trace_t *results, int max_results);
void wave_index_free(wave_index_t *idx);
#endif
