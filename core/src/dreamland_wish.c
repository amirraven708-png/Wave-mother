#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dreamland.h"

/* convert our internal radian phase to the integer stored in wd_trace_t (0..10000) */
static uint64_t phase_to_int(double phase_rad) {
    double norm = fmod(phase_rad, 2.0 * M_PI);
    if (norm < 0) norm += 2.0 * M_PI;
    return (uint64_t)(norm / (2.0 * M_PI) * 10000.0) % 10000;
}


/* ── Exact search ── */
static int wish_exact(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results) {
    int found = 0;
    for (size_t i = 0; i < dl->trace_count && found < max_results; i++) {
        if (dl->traces[i].rhythm == wish->rhythm) {
            results[found].type = DREAM_RESULT_EXACT;
            results[found].confidence = 1.0;
            results[found].resonance = 1.0;
            results[found].source_ids[0] = dl->traces[i].exact_key;
            results[found].source_count = 1;
            results[found].persistent = 1;
            results[found].transmitted = 0;
            strncpy(results[found].content, dl->traces[i].content, sizeof(results[found].content)-1);
            found++;
        }
    }
    return found;
}

/* ── Character overlap hint (simple semantic) ── */
static int wish_semantic(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results) {
    int found = 0;
    for (size_t i = 0; i < dl->trace_count && found < max_results; i++) {
        double score = 0.0;
        for (int j = 0; j < wish->payload_len && j < 16; j++) {
            if (strchr(dl->traces[i].content, wish->payload[j])) score += 0.1;
        }
        if (score > 0.2) {
            results[found].type = DREAM_RESULT_RESONANT;
            results[found].confidence = fmin(score, 1.0);
            results[found].resonance = score;
            results[found].source_ids[0] = dl->traces[i].exact_key;
            results[found].source_count = 1;
            results[found].persistent = 1;
            results[found].transmitted = 0;
            strncpy(results[found].content, dl->traces[i].content, sizeof(results[found].content)-1);
            found++;
        }
    }
    return found;
}

/* ── Pattern search using wave_index ── */
static int wish_pattern(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results) {
    if (!dl->wave_index) return 0;
    uint64_t pmin = phase_to_int(fmax(wish->phase - wish->phase_tolerance, 0.0));
    uint64_t pmax = phase_to_int(fmin(wish->phase + wish->phase_tolerance, 2.0 * M_PI));
    wd_trace_t temp[64];
    int n = wave_index_select(dl->wave_index, wish->rhythm, wish->rhythm_mask, pmin, pmax,
                              temp, max_results);
    for (int i = 0; i < n; i++) {
        results[i].type = DREAM_RESULT_RESONANT;
        results[i].confidence = 0.8;
        results[i].resonance = 0.8;
        results[i].source_ids[0] = temp[i].exact_key;
        results[i].source_count = 1;
        results[i].persistent = 1;
        results[i].transmitted = 0;
        strncpy(results[i].content, temp[i].content, sizeof(results[i].content)-1);
    }
    return n;
}

/* ── Ingenuity: combine resonance spaces ── */
static int wish_ingenuity(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results) {
    resonance_space_t spaces[MAX_RESONANCE];
    int n_spaces = dreamland_find_resonance(dl, wish, spaces, MAX_RESONANCE);
    if (n_spaces == 0) return 0;
    char combined[512];
    dreamland_combine_temporary(spaces, n_spaces, combined, sizeof(combined));
    results[0].type = DREAM_RESULT_COMPOSED;
    results[0].confidence = 0.6;
    results[0].resonance = 0.6;
    for (int i = 0; i < n_spaces; i++) {
        results[0].source_ids[i] = spaces[i].source_rhythm;  // using rhythm as source id
    }
    results[0].source_count = n_spaces;
    results[0].persistent = 0;
    results[0].transmitted = 0;
    strncpy(results[0].content, combined, sizeof(results[0].content)-1);
    return 1;
}

/* ── Main wish dispatcher ── */
int dreamland_wish(dreamland_t *dl, const wish_t *wish, dream_result_t *results, int max_results) {
    if (wish->precision > 0.8) {
        int n = wish_exact(dl, wish, results, max_results);
        if (n > 0) return n;
        if (wish->ingenuity > 0.5) return wish_ingenuity(dl, wish, results, max_results);
        return wish_pattern(dl, wish, results, max_results);
    }
    if (wish->ingenuity > 0.7) {
        int n = wish_semantic(dl, wish, results, max_results);
        if (n > 0) return n;
        return wish_ingenuity(dl, wish, results, max_results);
    }
    /* default: pattern search */
    return wish_pattern(dl, wish, results, max_results);
}
