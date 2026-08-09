#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wave_fabric.h"
#include "need_signal.h"
#include "radio_port.h"
#include "wave_return_trace.h"

extern radio_t rp;
extern ns_t *sig;
static uint64_t wave_task_counter = 1;

int wave_fabric_submit(wave_task_t *task) {
    if (!task) return -1;
    task->task_id = wave_task_counter++;
    printf("[Fabric] Offload task %lu (capacity=%lu, rhythm=%lu)\n",
           task->task_id, task->required_capacity, task->rhythm);

    if (task->required_capacity > 0 && !sig) {
        sig = ns_trigger(0xAAAA1111, task->required_capacity, 0);
        if (sig) {
            radio_cast(&rp, MSG_NEED, 0xAAAA1111, task->rhythm,
                      0.0, 1.0, sig->def, "OFFLOAD_NEED");
        }
    }
    return 0;
}

int wave_fabric_receive_result(uint64_t task_id, void *buffer, uint32_t timeout_ms) {
    int fake_result = 42;
    return_trace_t *trace = trace_create(task_id, 0xBBBB2222, &fake_result, sizeof(int), 0);
    if (!trace) return -1;

    if (buffer) {
        memcpy(buffer, trace, sizeof(return_trace_t));
    }
    trace_free(trace);
    return 0;
}
