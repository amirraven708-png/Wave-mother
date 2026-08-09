#ifndef RADIO_PROTOCOL_H
#define RADIO_PROTOCOL_H
#include <stdint.h>
typedef enum { RADIO_MSG_NEED_SIGNAL=1, RADIO_MSG_WAVE_SUPPLY=2, RADIO_MSG_PHASE_BEACON=3 } radio_msg_type_t;
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0x52414449 "RADI"
    uint8_t  msg_type;
    uint64_t sender_node_id;
    uint64_t target_node_id;
    uint64_t payload_val;     // deficit / capacity / phase*1000
    char     data[64];
} radio_packet_t;
#define RADIO_MAGIC 0x52414449
#endif
