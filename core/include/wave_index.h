#ifndef WAVE_INDEX_H
#define WAVE_INDEX_H
#include <stdint.h>
#include <stddef.h>
#include "wave_trace.h"

typedef struct { size_t count, capacity, *indices; } bucket_t;

typedef struct {
    /* phase / rhythm index */
    bucket_t *buckets_rhythm;
    size_t    num_buckets_rhythm;

    /* exact‑key index */
    bucket_t *buckets_key;
    size_t    num_buckets_key;

    const wd_trace_t *traces;
    size_t trace_count;
} wave_index_t;

int  wave_index_build(wave_index_t *idx, const wd_trace_t *t, size_t n, int nb);
void wave_index_free(wave_index_t *idx);

int  wave_index_get_by_key(const wave_index_t *idx, uint64_t key, wd_trace_t *out);
int  wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t mask,
                       uint64_t pmin, uint64_t pmax, wd_trace_t *res, int max);
int  wave_index_select_by_phase(const wave_index_t *idx, float target, float tol,
                                wd_trace_t *res, int max);
#endif
