#ifndef WAVE_QUERY_H
#define WAVE_QUERY_H
#include <stdint.h>
#include "wave_trace.h"

typedef struct {
    uint64_t rhythm;
    uint64_t mask;
    uint64_t phase_min;
    uint64_t phase_max;
    uint32_t type;
} wave_query_t;
#endif
