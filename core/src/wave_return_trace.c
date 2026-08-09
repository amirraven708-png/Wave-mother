#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wave_return_trace.h"

return_trace_t* trace_create(uint64_t task_id, uint64_t executor_id,
                             const void *data, uint32_t data_size, uint32_t exit_code) {
    return_trace_t *t = malloc(sizeof(return_trace_t));
    if (!t) return NULL;
    t->task_id = task_id;
    t->executor_id = executor_id;
    t->exit_code = exit_code;
    t->data_size = data_size < sizeof(t->data) ? data_size : sizeof(t->data);
    if (data && t->data_size > 0) memcpy(t->data, data, t->data_size);
    // محاسبهٔ هش ساده
    t->result_hash = 0;
    for (uint32_t i = 0; i < t->data_size; i++) {
        t->result_hash = (t->result_hash * 31) + t->data[i];
    }
    return t;
}

void trace_free(return_trace_t *t) {
    free(t);
}
