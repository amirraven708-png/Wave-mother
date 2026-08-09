#ifndef MANIFEST_VOLUMES_H
#define MANIFEST_VOLUMES_H

#include <stdint.h>
#include "wave_trace.h"

#define NUM_VOLUMES        5
#define MAX_MANIFEST_PATHS 1024

typedef struct {
    uint64_t id;
    double   curvature;
    uint64_t base_bits;
    double   resonance_freq;
} ManifestVolume;

typedef struct {
    uint64_t source_vol;
    uint64_t target_vol;
    double   path_weight;
    double   phase_alignment;
    int      is_traced;
} ManifestPath;

typedef struct {
    ManifestVolume volumes[NUM_VOLUMES];
    ManifestPath   all_paths[MAX_MANIFEST_PATHS];
    int            path_count;
    int            is_frozen;
    double         global_phase;
    wd_trace_t     *solid_traces;
    size_t         solid_count;
} BlueBusSolidCore;

void manifest_init(BlueBusSolidCore *core);
void trace_all_manifest_paths(BlueBusSolidCore *core);
int  apply_throat_compression(BlueBusSolidCore *core);
int  check_observer_sync(const BlueBusSolidCore *core);

#endif
