#ifndef WAVE_ELLIPSE_MEMORY_H
#define WAVE_ELLIPSE_MEMORY_H

#include "wave_behavior_policy.h"

typedef struct {
    double focus_a;
    double focus_b;
    double eccentricity;
    double saved_scores[MAX_ACTIONS];
    int is_solidified;
} holographic_ellipse_memory_t;

void ellipse_memory_init(holographic_ellipse_memory_t *mem);
int ellipse_freeze_behavior(holographic_ellipse_memory_t *mem, behavior_learner_t *learner);
int ellipse_thaw_behavior(const holographic_ellipse_memory_t *mem, behavior_learner_t *learner);

#endif
