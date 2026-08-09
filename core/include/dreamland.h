#ifndef DREAMLAND_H
#define DREAMLAND_H

#include <stdint.h>
#include "wave_index.h"
#include "dreamland_wish.h"

#define MAX_VESSELS     16
#define MAX_SIGNALS     256
#define MAX_RESONANCE   32
#define MAX_PAYLOAD     256
#define MAX_RULES       16

typedef struct {
    uint64_t id;
    uint64_t timestamp;
    double   intensity;
    double   frequency;
    double   phase;          // radians
    char     payload[MAX_PAYLOAD];
    int      payload_len;
} signal_t;

typedef struct {
    uint64_t id;
    double   receptivity_base;
    double   relevance_window;  // seconds
    double   phase_selectivity; // 0 = any, 1 = exact
    double   target_phase;      // radians
    double   noise_threshold;
    uint64_t last_reception;
    double   current_fill;      // 0 .. 1
    int      is_active;
} vessel_t;

typedef struct {
    uint64_t source_rhythm;
    double   resonance_strength;
    double   phase_alignment;
    int      is_temporary;
    char     combined_content[512];
} resonance_space_t;

typedef struct {
    uint64_t source_transmission;
    uint64_t first_seen;
    uint64_t last_seen;
    double   stability;
    double   propagation;
    double   confidence;
    int      promoted_to_rule;
} wave_rule_candidate_t;

typedef struct {
    vessel_t              vessels[MAX_VESSELS];
    int                   vessel_count;
    signal_t              signal_buffer[MAX_SIGNALS];
    int                   signal_count;
    resonance_space_t     resonance_spaces[MAX_RESONANCE];
    int                   resonance_count;
    wave_index_t         *wave_index;
    wd_trace_t           *traces;
    size_t                trace_count;
    wave_rule_candidate_t candidates[MAX_RULES];
    int                   candidate_count;
    uint64_t              current_time;  // monotonic, ns
} dreamland_t;

void dreamland_init(dreamland_t *dl);
int  dreamland_add_vessel(dreamland_t *dl, double receptivity, double relevance_window,
                          double phase_selectivity, double target_phase, double noise_threshold);
int  dreamland_receive_signal(dreamland_t *dl, const signal_t *sig);
void dreamland_process(dreamland_t *dl);
void dreamland_report(const dreamland_t *dl);

/* wish processing */
int  dreamland_wish(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results);

/* resonance helpers */
int  dreamland_find_resonance(dreamland_t *dl, const wish_t *wish,
                              resonance_space_t *spaces, int max_spaces);
int  dreamland_combine_temporary(const resonance_space_t *spaces, int space_count,
                                 char *output, int max_output_len);

/* utility: current monotonic time in ns */
uint64_t dreamland_now_ns(void);

#endif
