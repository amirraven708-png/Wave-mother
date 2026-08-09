#ifndef WAVE_BEHAVIOR_POLICY_H
#define WAVE_BEHAVIOR_POLICY_H

#include "wave_state_field.h"

#define MAX_ACTIONS 5

typedef enum {
    ACTION_LISTENING = 0,
    ACTION_GENERATING_WAVES = 1,
    ACTION_ADJUSTING_PHASE = 2,
    ACTION_SHARING_DIMENSIONS = 3,
    ACTION_SLEEP = 4
} system_action_t;

typedef struct {
    system_action_t action_id;
    double cumulative_score;
    int selection_count;
} action_policy_t;

typedef struct {
    action_policy_t actions[MAX_ACTIONS];
    wave_state_t last_state;
    double weight_c;   // reward for coherence increase
    double weight_r;   // penalty for residual increase
    double weight_n;   // penalty for dimensional expansion
} behavior_learner_t;

void learner_init(behavior_learner_t *learner);
system_action_t learner_choose_action(behavior_learner_t *learner, double current_need);
void learner_evaluate_feedback(behavior_learner_t *learner, system_action_t last_action, wave_state_t current_state);

#endif
