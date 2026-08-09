#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "passive_discovery.h"

#define PHASE_MAX 10000

int main() {
    int sock = setup_multicast_receiver();
    if (sock < 0) { perror("receiver"); return 1; }

    uint64_t my_phase = 5000;
    int neighbor_seen = 0;

    printf("Node A (Consumer) listening on multicast...\n");

    while (1) {
        wave_packet_t pkt;
        if (recv_packet(sock, &pkt) > 0) {
            if (pkt.type == 0) {
                printf("Discovered Node 0x%lx with phase %lu\n", pkt.node_id, pkt.phase);
                neighbor_seen = 1;
            } else if (pkt.type == 1 && neighbor_seen) {
                int diff = (int)(pkt.phase - my_phase + PHASE_MAX) % PHASE_MAX;
                if (diff > PHASE_MAX/2) diff -= PHASE_MAX;
                my_phase = (my_phase + diff / 10) % PHASE_MAX;
                printf("Sync: new phase = %lu (diff=%d)\n", my_phase, diff);
            }
        }
        usleep(10000);
    }
    close(sock);
    return 0;
}
