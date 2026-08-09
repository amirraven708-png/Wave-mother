#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "passive_discovery.h"

#define PHASE_MAX 10000

int main() {
    int sock = setup_multicast_sender();
    if (sock < 0) { perror("sender"); return 1; }

    uint64_t my_id = 0xABCD1234;
    uint64_t phase = 0;

    printf("Node B (Producer) broadcasting...\n");

    while (1) {
        send_discovery(sock, my_id, phase);
        send_sync(sock, my_id, phase);
        phase = (phase + 1) % PHASE_MAX;
        usleep(100000);
    }
    close(sock);
    return 0;
}
