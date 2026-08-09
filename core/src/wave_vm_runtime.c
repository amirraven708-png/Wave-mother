#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wave_vm.h"

int wave_vm_init(wave_vm_context_t *ctx) {
    if (!ctx) return -1;
    memset(ctx->registers, 0, sizeof(ctx->registers));
    ctx->linear_memory = (uint8_t *)calloc(WAVE_VM_MEMORY_SIZE, 1);
    if (!ctx->linear_memory) return -1;
    ctx->program_counter = 0;
    ctx->is_running = 1;
    ctx->fabric_context = NULL;
    return 0;
}

int wave_vm_execute(wave_vm_context_t *ctx,
                    const wave_instruction_t *program,
                    size_t inst_count) {
    while (ctx->is_running && ctx->program_counter < inst_count) {
        wave_instruction_t inst = program[ctx->program_counter++];
        switch (inst.opcode) {
        case WAVE_OP_NOP: break;
        case WAVE_OP_LOAD_I: ctx->registers[inst.reg_a] = inst.operand_c; break;
        case WAVE_OP_ADD:
            ctx->registers[inst.reg_a] = ctx->registers[inst.reg_b] + ctx->registers[inst.operand_c];
            break;
        case WAVE_OP_SUB:
            ctx->registers[inst.reg_a] = ctx->registers[inst.reg_b] - ctx->registers[inst.operand_c];
            break;
        case WAVE_OP_MUL:
            ctx->registers[inst.reg_a] = ctx->registers[inst.reg_b] * ctx->registers[inst.operand_c];
            break;
        case WAVE_OP_DIV:
            if (ctx->registers[inst.operand_c])
                ctx->registers[inst.reg_a] = ctx->registers[inst.reg_b] / ctx->registers[inst.operand_c];
            break;
        case WAVE_OP_OFFLOAD: {
            wave_task_t task;
            task.task_id = ctx->registers[0];        // R0 = Task ID
            task.required_capacity = ctx->registers[1]; // R1 = نیاز به ظرفیت
            task.rhythm = ctx->registers[2];          // R2 = ریتم
            task.priority = 1.0;
            task.transient = 1;
            wave_fabric_submit(&task);
            break;
        }
        case WAVE_OP_HALT: ctx->is_running = 0; break;
        default: printf("Unknown opcode 0x%X\n", inst.opcode); ctx->is_running = 0;
        }
    }
    return 0;
}

void wave_vm_destroy(wave_vm_context_t *ctx) {
    if (ctx) { free(ctx->linear_memory); ctx->linear_memory = NULL; }
}

uint64_t wave_vm_get_register(wave_vm_context_t *ctx, int idx) {
    return (ctx && idx >= 0 && idx < WAVE_VM_REGISTERS) ? ctx->registers[idx] : 0;
}
