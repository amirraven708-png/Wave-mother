#include <string.h>
#include "wave_selector.h"

int wave_match(const wave_trace_t* t, const wave_query_t* q) {
    if ((t->rhythm & q->mask) != (q->rhythm & q->mask)) return 0;
    if (t->phase < q->phase_min || t->phase > q->phase_max) return 0;
    if (q->type != 0 && t->type != q->type) return 0;
    return 1;
}

int wave_select(const wave_memory_t* mem, const wave_query_t* query,
                wave_trace_t* results, size_t max_results) {
    size_t count = 0;
    for (size_t i = 0; i < mem->capacity; i++) {
        wave_trace_t* t = &mem->traces[i];
        if (t->content && wave_match(t, query)) {
            if (count < max_results) {
                memcpy(&results[count], t, sizeof(wave_trace_t));
                results[count].content = t->content;
            }
            count++;
        }
    }
    return (int)count;
}
