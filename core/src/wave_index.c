#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "wave_index.h"

/* ── hash helpers ── */
static inline size_t hash_rhythm(uint64_t r, size_t nb) {
    uint16_t key = (r >> 48) & 0xFFFF;
    return key % nb;
}
static inline size_t hash_exact(uint64_t k, size_t nb) {
    uint64_t h = k;
    h ^= h >> 33; h *= 0xff51afd7ed558ccd;
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h % nb;
}

static int alloc_buckets(bucket_t **buckets, size_t nb) {
    *buckets = calloc(nb, sizeof(bucket_t));
    if (!*buckets) return -1;
    return 0;
}

int wave_index_build(wave_index_t *idx, const wd_trace_t *t, size_t n, int nb) {
    if (!idx || nb <= 0) return -1;
    memset(idx, 0, sizeof(*idx));
    idx->trace_count = n;
    idx->traces = t;
    idx->num_buckets_rhythm = (size_t)nb;
    idx->num_buckets_key    = (size_t)nb;

    if (alloc_buckets(&idx->buckets_rhythm, idx->num_buckets_rhythm) < 0) return -1;
    if (alloc_buckets(&idx->buckets_key,    idx->num_buckets_key)    < 0) {
        free(idx->buckets_rhythm); return -1;
    }

    /* first pass – count */
    for (size_t i = 0; i < n; i++) {
        idx->buckets_rhythm[hash_rhythm(t[i].rhythm, idx->num_buckets_rhythm)].count++;
        idx->buckets_key[hash_exact(t[i].exact_key, idx->num_buckets_key)].count++;
    }

    /* allocate arrays */
    for (size_t i = 0; i < idx->num_buckets_rhythm; i++) {
        bucket_t *b = &idx->buckets_rhythm[i];
        if (b->count) { b->capacity = b->count; b->indices = malloc(b->count * sizeof(size_t)); b->count = 0; }
    }
    for (size_t i = 0; i < idx->num_buckets_key; i++) {
        bucket_t *b = &idx->buckets_key[i];
        if (b->count) { b->capacity = b->count; b->indices = malloc(b->count * sizeof(size_t)); b->count = 0; }
    }

    /* second pass – fill */
    for (size_t i = 0; i < n; i++) {
        size_t br = hash_rhythm(t[i].rhythm, idx->num_buckets_rhythm);
        idx->buckets_rhythm[br].indices[idx->buckets_rhythm[br].count++] = i;
        size_t bk = hash_exact(t[i].exact_key, idx->num_buckets_key);
        idx->buckets_key[bk].indices[idx->buckets_key[bk].count++] = i;
    }
    return 0;
}

void wave_index_free(wave_index_t *idx) {
    if (!idx) return;
    for (int k = 0; k < 2; k++) {
        bucket_t *buckets = k ? idx->buckets_key : idx->buckets_rhythm;
        size_t    nb      = k ? idx->num_buckets_key : idx->num_buckets_rhythm;
        if (!buckets) continue;
        for (size_t i = 0; i < nb; i++) free(buckets[i].indices);
        free(buckets);
    }
    memset(idx, 0, sizeof(*idx));
}

/* ── FAST exact‑key lookup ── */
int wave_index_get_by_key(const wave_index_t *idx, uint64_t key, wd_trace_t *out) {
    if (!idx || !out) return 0;
    size_t b = hash_exact(key, idx->num_buckets_key);
    bucket_t *bk = &idx->buckets_key[b];
    for (size_t i = 0; i < bk->count; i++) {
        size_t ti = bk->indices[i];
        if (idx->traces[ti].exact_key == key) { *out = idx->traces[ti]; return 1; }
    }
    return 0;
}

/* ── masked rhythm search ── */
int wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t mask,
                      uint64_t pmin, uint64_t pmax, wd_trace_t *res, int max) {
    if (!idx || !res || max <= 0) return 0;
    size_t b = hash_rhythm(rhythm, idx->num_buckets_rhythm);
    bucket_t *bk = &idx->buckets_rhythm[b];
    int found = 0;
    for (size_t i = 0; i < bk->count && found < max; i++) {
        const wd_trace_t *t = &idx->traces[bk->indices[i]];
        if ((t->rhythm & mask) != (rhythm & mask)) continue;
        if (t->phase < (double)pmin || t->phase > (double)pmax) continue;
        res[found++] = *t;
    }
    return found;
}

/* ── phase scan (linear – acceptable for rare queries) ── */
int wave_index_select_by_phase(const wave_index_t *idx, float target, float tol,
                               wd_trace_t *res, int max) {
    if (!idx || !res) return 0;
    int found = 0;
    for (size_t i = 0; i < idx->trace_count && found < max; i++) {
        float diff = (float)idx->traces[i].phase - target;
        if (diff < 0) diff = -diff;
        if (diff <= tol) res[found++] = idx->traces[i];
    }
    return found;
}
