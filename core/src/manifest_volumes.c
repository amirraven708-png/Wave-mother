#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "manifest_volumes.h"

void manifest_init(BlueBusSolidCore *core) {
    memset(core, 0, sizeof(*core));
    double curvatures[NUM_VOLUMES] = {1.0, 1.5, 0.8, 2.0, 1.2};
    uint64_t bits[NUM_VOLUMES]    = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F};
    double freqs[NUM_VOLUMES]     = {440.0, 528.0, 639.0, 741.0, 852.0};
    for (int i = 0; i < NUM_VOLUMES; i++) {
        core->volumes[i].id = i;
        core->volumes[i].curvature = curvatures[i];
        core->volumes[i].base_bits = bits[i];
        core->volumes[i].resonance_freq = freqs[i];
    }
    core->global_phase = 0.0;
    printf("[Manifest] Pentagonal core initialized with 5 volumes.\n");
}

void trace_all_manifest_paths(BlueBusSolidCore *core) {
    core->path_count = 0;
    for (int i = 0; i < NUM_VOLUMES; i++) {
        for (int j = i + 1; j < NUM_VOLUMES; j++) {
            if (core->path_count >= MAX_MANIFEST_PATHS) break;
            ManifestPath *p1 = &core->all_paths[core->path_count++];
            p1->source_vol = i; p1->target_vol = j;
            p1->path_weight = 1.0 / (1.0 + fabs(core->volumes[i].curvature - core->volumes[j].curvature));
            p1->phase_alignment = cos(core->volumes[i].resonance_freq - core->volumes[j].resonance_freq);
            p1->is_traced = 1;
            if (core->path_count >= MAX_MANIFEST_PATHS) break;
            ManifestPath *p2 = &core->all_paths[core->path_count++];
            p2->source_vol = j; p2->target_vol = i;
            p2->path_weight = p1->path_weight;
            p2->phase_alignment = p1->phase_alignment;
            p2->is_traced = 1;
        }
    }
    printf("[Manifest] All %d possible paths traced. Redundancy: ABSOLUTE.\n", core->path_count);
}

int apply_throat_compression(BlueBusSolidCore *core) {
    uint64_t compressed_bits = 0;
    for (int i = 0; i < NUM_VOLUMES; i++) compressed_bits |= core->volumes[i].base_bits;
    if (compressed_bits == 0xFF) {
        core->is_frozen = 1;
        core->global_phase = 0.0;
        core->solid_traces = malloc(core->path_count * sizeof(wd_trace_t));
        core->solid_count = core->path_count;
        for (int i = 0; i < core->path_count; i++) {
            wd_trace_t *t = &core->solid_traces[i];
            t->exact_key = (core->all_paths[i].source_vol << 32) | core->all_paths[i].target_vol;
            t->rhythm = compressed_bits;
            t->phase = core->all_paths[i].phase_alignment * 180.0;
            t->type = 1; t->size = 0;
        }
        printf("[Manifest] THROAT COMPRESSION achieved. Time frozen. %zu solid traces created.\n", core->solid_count);
        return 1;
    }
    printf("[Manifest] Compression level: 0x%lX – threshold not reached yet.\n", compressed_bits);
    return 0;
}

int check_observer_sync(const BlueBusSolidCore *core) {
    if (!core->is_frozen) return 0;
    int all_traced = 1;
    for (int i = 0; i < core->path_count; i++) if (!core->all_paths[i].is_traced) { all_traced = 0; break; }
    if (all_traced) { printf("[Observers] Both observers confirm: ALL PATHS KNOWN. TIME IS FROZEN.\n"); return 1; }
    return 0;
}
