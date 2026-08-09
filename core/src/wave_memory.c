#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "wave_memory.h"

int wave_memory_init(wave_memory_t* mem, size_t max_traces) {
    mem->traces = calloc(max_traces, sizeof(wave_trace_t));
    if (!mem->traces) return -1;
    mem->capacity = max_traces;
    mem->head = 0;
    mem->count = 0;
    return 0;
}

void wave_memory_free(wave_memory_t* mem) {
    for (size_t i = 0; i < mem->capacity; i++) free(mem->traces[i].content);
    free(mem->traces);
}

int wave_emit(wave_memory_t* mem, uint64_t rhythm, uint64_t phase,
              uint32_t type, const void* content, uint32_t size) {
    if (mem->count >= mem->capacity) {
        size_t oldest = (mem->head + mem->capacity - mem->count) % mem->capacity;
        free(mem->traces[oldest].content);
        mem->traces[oldest].content = NULL;
        mem->count--;
    }
    wave_trace_t* t = &mem->traces[mem->head];
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    t->position  = mem->head;
    t->rhythm    = rhythm;
    t->phase     = phase;
    t->type      = type;
    t->size      = size;
    t->content   = malloc(size);
    if (!t->content) return -1;
    memcpy(t->content, content, size);
    mem->head = (mem->head + 1) % mem->capacity;
    mem->count++;
    return 0;
}

void wave_memory_dump(const wave_memory_t* mem) {
    printf("Wave Memory [%zu/%zu traces]:\n", mem->count, mem->capacity);
    for (size_t i = 0; i < mem->capacity; i++) {
        wave_trace_t* t = &mem->traces[i];
        if (t->content)
            printf("  [%zu] time=%lu rhythm=0x%016lx phase=0x%016lx\n",
                   i, t->timestamp, t->rhythm, t->phase);
    }
}
