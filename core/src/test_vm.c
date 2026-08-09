#include <stdio.h>
#include "wave_vm.h"
#include "wave_return_trace.h"

int main() {
    wave_vm_context_t vm;
    wave_vm_init(&vm);

    wave_instruction_t prog[] = {
        {WAVE_OP_LOAD_I, 1, 0, 10},
        {WAVE_OP_LOAD_I, 2, 0, 20},
        {WAVE_OP_ADD,    3, 1, 2},
        {WAVE_OP_OFFLOAD,0, 1, 2},
        {WAVE_OP_HALT,   0, 0, 0}
    };

    wave_vm_execute(&vm, prog, 5);
    printf("R3 = %lu (expected 30)\n", wave_vm_get_register(&vm, 3));

    return_trace_t result;
    if (wave_fabric_receive_result(1, &result, 1000) == 0) {
        printf("[Return] Task %lu completed by node %lX, exit=%u, data=",
               result.task_id, result.executor_id, result.exit_code);
        for (uint32_t i = 0; i < result.data_size; i++) {
            printf("%02X ", result.data[i]);
        }
        printf("\n");
    }

    wave_vm_destroy(&vm);
    return 0;
}
