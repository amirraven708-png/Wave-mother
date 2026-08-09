#ifndef WAVE_VM_H
#define WAVE_VM_H
#include <stdint.h>
#include "wave_ir.h"
#include "wave_fabric.h"

#define WAVE_VM_REGISTERS    32
#define WAVE_VM_MEMORY_SIZE  (16 * 1024 * 1024)

typedef struct {
    uint64_t registers[WAVE_VM_REGISTERS];
    uint8_t  *linear_memory;
    uint32_t  program_counter;
    uint8_t   is_running;
    void     *fabric_context;  // اتصال به Fabric
} wave_vm_context_t;

int      wave_vm_init(wave_vm_context_t *ctx);
int      wave_vm_execute(wave_vm_context_t *ctx, const wave_instruction_t *prog, size_t count);
void     wave_vm_destroy(wave_vm_context_t *ctx);
uint64_t wave_vm_get_register(wave_vm_context_t *ctx, int idx);
#endif
