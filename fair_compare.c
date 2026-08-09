#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_KEYS      100000
#define PAYLOAD_SIZE  64
#define LOOKUPS       50000

/* ---------- simple hash table (Redis-like) ---------- */
typedef struct kv_s { uint64_t key; char val[PAYLOAD_SIZE]; struct kv_s *next; } kv_t;

typedef struct { kv_t **buckets; size_t nb; } ht_t;

void ht_init(ht_t *h, size_t nb) { h->nb = nb; h->buckets = calloc(nb, sizeof(kv_t*)); }

void ht_insert(ht_t *h, uint64_t key, const char *val) {
    size_t b = key % h->nb;
    kv_t *e = malloc(sizeof(kv_t)); e->key = key;
    memcpy(e->val, val, PAYLOAD_SIZE); e->next = h->buckets[b]; h->buckets[b] = e;
}

char* ht_lookup(ht_t *h, uint64_t key) {
    for (kv_t *e = h->buckets[key % h->nb]; e; e = e->next)
        if (e->key == key) return e->val;
    return NULL;
}

void ht_free(ht_t *h) {
    for (size_t i = 0; i < h->nb; i++) { kv_t *e = h->buckets[i]; while (e) { kv_t *n = e->next; free(e); e = n; } }
    free(h->buckets);
}

/* ---------- wave index (same as core) ---------- */
#include "wave_trace.h"
#include "wave_index.h"

/* ---------- main benchmark ---------- */
int main() {
    srand(12345);
    uint64_t *keys = malloc(NUM_KEYS * sizeof(uint64_t));
    char **vals = malloc(NUM_KEYS * sizeof(char*));
    for (int i = 0; i < NUM_KEYS; i++) { keys[i] = ((uint64_t)rand() << 32) | rand(); vals[i] = malloc(PAYLOAD_SIZE);
        for (int j = 0; j < PAYLOAD_SIZE; j++) vals[i][j] = rand() & 0xFF; }

    printf("=== FAIR COMPARISON: HashTable vs WaveIndex ===\n");
    printf("%d keys, %d lookups\n", NUM_KEYS, LOOKUPS);

    // --- Hash Table ---
    ht_t ht; ht_init(&ht, 100003);
    clock_t t0 = clock();
    for (int i = 0; i < NUM_KEYS; i++) ht_insert(&ht, keys[i], vals[i]);
    double ht_ins = (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0;

    t0 = clock();
    int ht_found = 0;
    for (int i = 0; i < LOOKUPS; i++) if (ht_lookup(&ht, keys[rand() % NUM_KEYS])) ht_found++;
    double ht_look = (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0;

    // --- Wave Index ---
    wd_trace_t *traces = malloc(NUM_KEYS * sizeof(wd_trace_t));
    for (int i = 0; i < NUM_KEYS; i++) {
        traces[i].exact_key = keys[i];
        traces[i].rhythm = 0xABCD1234;
        traces[i].phase = (double)(rand() % 10000);
        traces[i].type = 1;
        traces[i].size = PAYLOAD_SIZE;
        memcpy(traces[i].content, vals[i], PAYLOAD_SIZE);
    }

    wave_index_t idx;
    t0 = clock();
    wave_index_build(&idx, traces, NUM_KEYS, 256);
    double wi_ins = (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0;

    t0 = clock();
    int wi_found = 0;
    wd_trace_t out;
    for (int i = 0; i < LOOKUPS; i++) {
        if (wave_index_get_by_key(&idx, keys[rand() % NUM_KEYS], &out)) wi_found++;
    }
    double wi_look = (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0;

    printf("\n%-15s %10s %10s\n", "Method", "Insert(ms)", "Lookup(ms)");
    printf("─────────────────────────────────\n");
    printf("%-15s %10.2f %10.2f\n", "Hash Table", ht_ins, ht_look);
    printf("%-15s %10.2f %10.2f\n", "Wave Index", wi_ins, wi_look);

    // cleanup
    ht_free(&ht);
    wave_index_free(&idx);
    free(traces);
    for (int i = 0; i < NUM_KEYS; i++) free(vals[i]);
    free(vals); free(keys);

    return 0;
}
