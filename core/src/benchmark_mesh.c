#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "wave_trace.h"
#include "wave_index.h"

#define P_MAX 1000
#define TRACES_PER_PRODUCER 1000

static void generate_traces(wd_trace_t *traces, size_t total, int P) {
    for (int p = 0; p < P; p++) {
        size_t offset = (size_t)p * TRACES_PER_PRODUCER;
        for (int i = 0; i < TRACES_PER_PRODUCER; i++) {
            size_t idx = offset + i;
            traces[idx].exact_key = idx;
            uint64_t rhythm = ((uint64_t)p << 48) | (i & 0x00FFFFFFFFFFFFFFULL);
            traces[idx].rhythm = rhythm;
            traces[idx].phase = (double)((i * 137) % 10000);
            traces[idx].type = 1;
            int val = p * 1000 + i;
            traces[idx].size = sizeof(val);
            memcpy(traces[idx].content, &val, sizeof(val));  // safe copy into array
        }
    }
}

int main(int argc, char **argv) {
    int P = P_MAX;
    if (argc > 1) P = atoi(argv[1]);
    if (P < 1) P = 1;
    size_t total = (size_t)P * TRACES_PER_PRODUCER;

    printf("=== MESH BENCHMARK (P=%d, %zu traces) ===\n", P, total);

    wd_trace_t *traces = calloc(total, sizeof(wd_trace_t));
    generate_traces(traces, total, P);

    wave_index_t idx;
    clock_t t0 = clock();
    wave_index_build(&idx, traces, total, 65536);
    clock_t t1 = clock();
    double t_build = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000.0;

    // Traditional scan
    volatile size_t trad_found = 0;
    t0 = clock();
    for (int c = 0; c < P; c++) {
        uint64_t target = (uint64_t)c << 48;
        for (size_t i = 0; i < total; i++) {
            if ((traces[i].rhythm & 0xFFFF000000000000ULL) == target) trad_found++;
        }
    }
    t1 = clock();
    double t_trad = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000.0;

    // Wave lookup using hash buckets
    volatile size_t wave_found = 0;
    t0 = clock();
    for (int c = 0; c < P; c++) {
        uint64_t target = (uint64_t)c << 48;
        wd_trace_t results[TRACES_PER_PRODUCER];
        int n = wave_index_select(&idx, target, 0xFFFF000000000000ULL, 0, 99999, results, TRACES_PER_PRODUCER);
        wave_found += n;
    }
    t1 = clock();
    double t_wave = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000.0;

    printf("Index build: %.2f ms\n", t_build);
    printf("Traditional scan: %.2f ms\n", t_trad);
    printf("Wave lookup: %.2f ms\n", t_wave);
    printf("Speedup: %.2f x\n", t_trad / t_wave);
    printf("Trad found: %zu, Wave found: %zu\n", trad_found, wave_found);

    wave_index_free(&idx);
    free(traces);   // content arrays are inside the struct, no separate free needed
    return 0;
}
