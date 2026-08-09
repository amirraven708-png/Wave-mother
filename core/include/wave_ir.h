#ifndef WAVE_IR_H
#define WAVE_IR_H
#include <stdint.h>

typedef enum {
    WAVE_OP_NOP=0x00, WAVE_OP_LOAD_I=0x10, WAVE_OP_LOAD_M=0x11,
    WAVE_OP_STORE=0x12, WAVE_OP_MOVE=0x13,
    WAVE_OP_ADD=0x20, WAVE_OP_SUB=0x21, WAVE_OP_MUL=0x22, WAVE_OP_DIV=0x23,
    WAVE_OP_AND=0x24, WAVE_OP_XOR=0x25,
    WAVE_OP_JMP=0x30, WAVE_OP_JEQ=0x31, WAVE_OP_CALL=0x32, WAVE_OP_RET=0x33,
    WAVE_OP_SYSCALL=0x40, WAVE_OP_OFFLOAD=0x41,
    WAVE_OP_HALT=0xFF
} wave_opcode_t;

typedef struct {
    uint8_t opcode;
    uint8_t reg_a;
    uint8_t reg_b;
    uint8_t operand_c;
} wave_instruction_t;

#endif
