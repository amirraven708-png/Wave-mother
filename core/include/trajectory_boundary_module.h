#ifndef TRAJECTORY_BOUNDARY_MODULE_H
#define TRAJECTORY_BOUNDARY_MODULE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t boundary_id;
    double   min_phase_amplitude;
    double   max_phase_amplitude;
    double   resonance_threshold;
    uint32_t active_constraint_flags;
} TrajectoryBoundary;

typedef struct {
    uint64_t           last_updated_timestamp;
    uint32_t           active_node_count;
    bool               is_locked_for_write;   /* writer sets this, reader spins */
    TrajectoryBoundary boundaries[256];
} TrajectorySharedState;

TrajectorySharedState* init_trajectory_memory_interface(const char *shm_name);
bool get_latest_trajectory_boundary(TrajectorySharedState *state,
                                    uint32_t node_id,
                                    TrajectoryBoundary *out_boundary);
void close_trajectory_memory_interface(TrajectorySharedState *state,
                                       const char *shm_name);

#endif
