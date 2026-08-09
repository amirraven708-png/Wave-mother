#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wave_seqlock_memory.h"

#ifndef SHM_FILE
#define SHM_FILE "/tmp/wave_shm.dat"
#endif

#define NODE_COUNT 10

typedef struct {
    double current_phase;
    uint64_t pulses;
} WaveState;

int main() {
    char shm_path[256];
    const char *home = getenv("HOME");
    if (home) snprintf(shm_path, sizeof(shm_path), "%s/wave_shm.dat", home);
    else snprintf(shm_path, sizeof(shm_path), SHM_FILE);

    printf("[Node-Ring] Booting 10-node fabric...\n");

    int fd = open(shm_path, O_RDONLY);
    if (fd < 0) {
        perror("[Node-Ring] open – run Python orchestrator first");
        return 1;
    }

    TrajectorySharedState *shared = mmap(NULL, sizeof(*shared),
                                         PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED) { perror("mmap"); return 1; }

    printf("[Node-Ring] Attached to heuristic boundary layer.\n");

    TrajectoryBoundary bounds[NODE_COUNT] = {0};
    WaveState nodes[NODE_COUNT] = {0};
    uint64_t tick = 0;

    while (1) {
        for (int i = 0; i < NODE_COUNT; i++) {
            read_trajectory_boundary(shared, i, &bounds[i]);
        }

        for (int i = 0; i < NODE_COUNT; i++) {
            if (nodes[i].current_phase > bounds[i].max_phase_amplitude)
                nodes[i].current_phase = bounds[i].max_phase_amplitude;
            else if (nodes[i].current_phase < bounds[i].min_phase_amplitude)
                nodes[i].current_phase = bounds[i].min_phase_amplitude;
            nodes[i].pulses++;
        }

        tick++;
        if (tick % 5000000 == 0) {
            printf("[Node-Ring] Tick %llu | Node0 bounds [%.2f, %.2f]\n",
                   (unsigned long long)tick,
                   bounds[0].min_phase_amplitude,
                   bounds[0].max_phase_amplitude);
        }
    }

    munmap(shared, sizeof(*shared));
    return 0;
}
