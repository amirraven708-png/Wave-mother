#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "trajectory_boundary_module.h"

TrajectorySharedState* init_trajectory_memory_interface(const char *shm_name) {
    /* استفاده از فایل معمولی به‌جای shm_open برای سازگاری با Termux */
    int fd = open(shm_name, O_RDONLY);
    if (fd < 0) {
        perror("open (reader)");
        return NULL;
    }

    TrajectorySharedState *state = mmap(NULL, sizeof(TrajectorySharedState),
                                        PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (state == MAP_FAILED) {
        perror("mmap (reader)");
        return NULL;
    }
    return state;
}

bool get_latest_trajectory_boundary(TrajectorySharedState *state,
                                    uint32_t node_id,
                                    TrajectoryBoundary *out_boundary) {
    if (!state || node_id >= 256) return false;

    while (__atomic_load_n(&state->is_locked_for_write, __ATOMIC_ACQUIRE))
        ;

    if (node_id >= state->active_node_count) return false;

    memcpy(out_boundary, &state->boundaries[node_id], sizeof(TrajectoryBoundary));
    return true;
}

void close_trajectory_memory_interface(TrajectorySharedState *state,
                                       const char *shm_name) {
    if (state) {
        munmap(state, sizeof(TrajectorySharedState));
    }
}
