#ifndef PASSIVE_DISCOVERY_H
#define PASSIVE_DISCOVERY_H

#include <stdint.h>

#define DISCOVERY_PORT 9999
#define MULTICAST_ADDR "224.0.0.251"

typedef struct {
    uint64_t node_id;
    uint64_t phase;
    uint32_t type;  // 0=discovery, 1=sync
} wave_packet_t;

int  setup_multicast_sender();
int  setup_multicast_receiver();
void send_discovery(int sock, uint64_t node_id, uint64_t phase);
void send_sync(int sock, uint64_t node_id, uint64_t phase);
int  recv_packet(int sock, wave_packet_t *pkt);

#endif
