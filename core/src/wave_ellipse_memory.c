#include <math.h>
#include <string.h>
#include <stdio.h>
#include "wave_ellipse_memory.h"

void ellipse_memory_init(holographic_ellipse_memory_t *mem) {
    if (!mem) return;
    mem->focus_a = 1.0;
    mem->focus_b = 0.5;
    mem->eccentricity = sqrt(1.0 - (mem->focus_b*mem->focus_b)/(mem->focus_a*mem->focus_a));
    mem->is_solidified = 0;
    memset(mem->saved_scores, 0, sizeof(mem->saved_scores));
}

int ellipse_freeze_behavior(holographic_ellipse_memory_t *mem, behavior_learner_t *learner) {
    if (!mem || !learner) return -1;
    for (int i = 0; i < MAX_ACTIONS; i++) {
        mem->saved_scores[i] = learner->actions[i].cumulative_score;
    }
    double total_energy = 0.0;
    for (int i = 0; i < MAX_ACTIONS; i++) total_energy += fabs(mem->saved_scores[i]);
    mem->focus_b = mem->focus_a * (1.0 / (1.0 + total_energy));
    mem->eccentricity = sqrt(1.0 - (mem->focus_b*mem->focus_b)/(mem->focus_a*mem->focus_a));
    mem->is_solidified = 1;
    printf("[EllipseMem] Behavior scores frozen on ellipse (ecc=%.3f).\n", mem->eccentricity);
    return 0;
}

int ellipse_thaw_behavior(const holographic_ellipse_memory_t *mem, behavior_learner_t *learner) {
    if (!mem || !learner || !mem->is_solidified) return -1;
    for (int i = 0; i < MAX_ACTIONS; i++) {
        learner->actions[i].cumulative_score = mem->saved_scores[i];
        if (learner->actions[i].selection_count == 0) learner->actions[i].selection_count = 1;
    }
    printf("[EllipseMem] Behavior scores thawed from ellipse (ecc=%.3f).\n", mem->eccentricity);
    return 0;
}
