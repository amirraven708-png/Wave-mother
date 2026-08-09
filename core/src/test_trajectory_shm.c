#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "trajectory_boundary_module.h"

#define SHM_FILE "/tmp/wave_trajectory_test.dat"

static void writer_simulate(void) {
    int fd = open(SHM_FILE, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(TrajectorySharedState));

    TrajectorySharedState *s = mmap(NULL, sizeof(*s),
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    memset(s, 0, sizeof(*s));

    s->active_node_count = 2;
    s->boundaries[0].boundary_id = 0;
    s->boundaries[0].min_phase_amplitude = 0.0;
    s->boundaries[0].max_phase_amplitude = 100.0;
    s->boundaries[0].resonance_threshold = 0.85;

    s->boundaries[1].boundary_id = 1;
    s->boundaries[1].min_phase_amplitude = 50.0;
    s->boundaries[1].max_phase_amplitude = 150.0;
    s->boundaries[1].resonance_threshold = 0.90;

    __atomic_store_n(&s->is_locked_for_write, 0, __ATOMIC_RELEASE);
    s->last_updated_timestamp = time(NULL);

    printf("Writer: boundaries published to %s\n", SHM_FILE);
    munmap(s, sizeof(*s));
}

int main() {
    writer_simulate();

    TrajectorySharedState *reader = init_trajectory_memory_interface(SHM_FILE);
    if (!reader) return 1;

    TrajectoryBoundary b;
    if (get_latest_trajectory_boundary(reader, 0, &b)) {
        printf("Node 0: min=%.2f max=%.2f thresh=%.2f\n",
               b.min_phase_amplitude, b.max_phase_amplitude,
               b.resonance_threshold);
    }
    if (get_latest_trajectory_boundary(reader, 1, &b)) {
        printf("Node 1: min=%.2f max=%.2f thresh=%.2f\n",
               b.min_phase_amplitude, b.max_phase_amplitude,
               b.resonance_threshold);
    }

    close_trajectory_memory_interface(reader, SHM_FILE);
    unlink(SHM_FILE);
    return 0;
}
