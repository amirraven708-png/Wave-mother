#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "wave_index.h"

static uint64_t hash_rhythm(uint64_t rhythm, size_t num_buckets) {
    uint64_t h = rhythm ^ (rhythm >> 32);
    h ^= h >> 16; h *= 0x85EBCA6B; h ^= h >> 13; h *= 0xC2B2AE35; h ^= h >> 16;
    return h % num_buckets;
}

void wave_index_build(wave_index_t *idx, const wd_trace_t *traces, size_t count, int num_buckets) {
    idx->num_buckets = (size_t)num_buckets;
    idx->num_traces = count;
    idx->traces = traces;
    idx->buckets = calloc(num_buckets, sizeof(rhythm_bucket_t));
    if (!idx->buckets) return;
    
    for (size_t i = 0; i < count; i++) {
        uint64_t h = hash_rhythm(traces[i].rhythm, idx->num_buckets);
        idx->buckets[h].count++;
    }
    
    for (size_t i = 0; i < idx->num_buckets; i++) {
        if (idx->buckets[i].count > 0) {
            idx->buckets[i].capacity = idx->buckets[i].count;
            idx->buckets[i].trace_indices = malloc(idx->buckets[i].count * sizeof(size_t));
            idx->buckets[i].count = 0;
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        uint64_t h = hash_rhythm(traces[i].rhythm, idx->num_buckets);
        idx->buckets[h].trace_indices[idx->buckets[h].count++] = i;
    }
}

int wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t rhythm_mask,
                      uint64_t phase_min, uint64_t phase_max,
                      wd_trace_t *results, int max_results) {
    if (!idx || !idx->buckets || !results) return 0;
    uint64_t h = hash_rhythm(rhythm, idx->num_buckets);
    rhythm_bucket_t *bucket = &idx->buckets[h];
    int found = 0;
    
    for (size_t i = 0; i < bucket->count && found < max_results; i++) {
        size_t ti = bucket->trace_indices[i];
        const wd_trace_t *t = &idx->traces[ti];
        if ((t->rhythm & rhythm_mask) != (rhythm & rhythm_mask)) continue;
        if (t->phase < (double)phase_min || t->phase > (double)phase_max) continue;
        results[found++] = *t;
    }
    return found;
}

void wave_index_free(wave_index_t *idx) {
    if (!idx || !idx->buckets) return;
    for (size_t i = 0; i < idx->num_buckets; i++) free(idx->buckets[i].trace_indices);
    free(idx->buckets);
    idx->buckets = NULL;
    idx->num_buckets = 0;
    idx->num_traces = 0;
}
