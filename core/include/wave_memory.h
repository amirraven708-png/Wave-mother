#ifndef WAVE_MEMORY_H
#define WAVE_MEMORY_H
#include <stddef.h>
#include "wave_trace.h"

typedef struct {
    wave_trace_t* traces;
    size_t        capacity;
    size_t        head;
    size_t        count;
} wave_memory_t;

int  wave_memory_init(wave_memory_t* mem, size_t max_traces);
void wave_memory_free(wave_memory_t* mem);
int  wave_emit(wave_memory_t* mem, uint64_t rhythm, uint64_t phase,
               uint32_t type, const void* content, uint32_t size);
void wave_memory_dump(const wave_memory_t* mem);
#endif
