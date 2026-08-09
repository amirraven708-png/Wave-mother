#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "dreamland.h"

uint64_t dreamland_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void dreamland_init(dreamland_t *dl) {
    memset(dl, 0, sizeof(*dl));
}

int dreamland_add_vessel(dreamland_t *dl, double receptivity, double relevance_window,
                          double phase_selectivity, double target_phase, double noise_threshold) {
    if (dl->vessel_count >= MAX_VESSELS) return -1;
    vessel_t *v = &dl->vessels[dl->vessel_count++];
    v->id = dl->vessel_count;
    v->receptivity_base = receptivity;
    v->relevance_window = relevance_window;
    v->phase_selectivity = phase_selectivity;
    v->target_phase = target_phase;
    v->noise_threshold = noise_threshold;
    v->last_reception = 0;
    v->current_fill = 0.0;
    v->is_active = 1;
    return 0;
}

/* internal helpers */
static double compute_receptivity(const vessel_t *v, uint64_t now) {
    (void)now;
    return v->receptivity_base * (1.0 - v->current_fill);
}

static double compute_relevance(const vessel_t *v, const signal_t *sig, uint64_t now) {
    double elapsed = (double)(now - sig->timestamp) * 1e-9;
    if (elapsed > v->relevance_window) return 0.0;
    double d = fabs(sig->phase - v->target_phase);
    if (d > M_PI) d = 2.0 * M_PI - d;
    double phase_match = 1.0 - (d / M_PI) * v->phase_selectivity;
    return phase_match * sig->intensity;
}

int dreamland_receive_signal(dreamland_t *dl, const signal_t *sig) {
    if (dl->signal_count >= MAX_SIGNALS) return -1;
    dl->signal_buffer[dl->signal_count++] = *sig;
    return 0;
}

void dreamland_process(dreamland_t *dl) {
    dl->current_time = dreamland_now_ns();
    for (int i = 0; i < dl->signal_count; i++) {
        signal_t *sig = &dl->signal_buffer[i];
        for (int j = 0; j < dl->vessel_count; j++) {
            vessel_t *v = &dl->vessels[j];
            if (!v->is_active) continue;
            if (sig->intensity < v->noise_threshold) continue;
            double r = compute_receptivity(v, dl->current_time) *
                       compute_relevance(v, sig, dl->current_time);
            if (r > 0.0) {
                v->current_fill = fmin(1.0, v->current_fill + r);
                v->last_reception = dl->current_time;
            }
        }
    }
    /* gradual decay */
    for (int j = 0; j < dl->vessel_count; j++) {
        vessel_t *v = &dl->vessels[j];
        double elapsed = (double)(dl->current_time - v->last_reception) * 1e-9;
        v->current_fill = fmax(0.0, v->current_fill - elapsed * 0.1);
    }
    dl->signal_count = 0;
}

void dreamland_report(const dreamland_t *dl) {
    printf("DREAMLAND STATUS\n");
    for (int i = 0; i < dl->vessel_count; i++) {
        const vessel_t *v = &dl->vessels[i];
        printf(" V%d fill=%.2f phase=%.2f active=%d\n",
               (int)v->id, v->current_fill, v->target_phase, v->is_active);
    }
}
