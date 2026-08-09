#ifndef WAVE_SEQLOCK_MEMORY_H
#define WAVE_SEQLOCK_MEMORY_H

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
    _Atomic uint32_t sequence_counter;
    TrajectoryBoundary data;
} SeqlockBoundary;

typedef struct {
    uint32_t        active_node_count;
    SeqlockBoundary nodes[256];
} TrajectorySharedState;

static inline bool read_trajectory_boundary(
    TrajectorySharedState *state,
    uint32_t node_id,
    TrajectoryBoundary *out_data)
{
    uint32_t seq1, seq2;
    SeqlockBoundary *target = &state->nodes[node_id];

    do {
        seq1 = __atomic_load_n(&target->sequence_counter, __ATOMIC_ACQUIRE);
        if (seq1 & 1) continue;
        *out_data = target->data;
        seq2 = __atomic_load_n(&target->sequence_counter, __ATOMIC_ACQUIRE);
    } while (seq1 != seq2);

    return true;
}

#endif
