#ifndef WAVE_SEQLOCK_MEMORY_H
#define WAVE_SEQLOCK_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

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
        seq1 = atomic_load(&target->sequence_counter);
        if (seq1 & 1) continue;          // writer active — retry
        *out_data = target->data;
        seq2 = atomic_load(&target->sequence_counter);
    } while (seq1 != seq2);              // writer touched data during copy — retry

    return true;
}

#endif
