#ifndef WAVE_SELECTOR_H
#define WAVE_SELECTOR_H
#include "wave_memory.h"
#include "wave_query.h"

int wave_match(const wave_trace_t* trace, const wave_query_t* query);
int wave_select(const wave_memory_t* mem, const wave_query_t* query,
                wave_trace_t* results, size_t max_results);
#endif
