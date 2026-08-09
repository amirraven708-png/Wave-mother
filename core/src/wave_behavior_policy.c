#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "wave_behavior_policy.h"

void learner_init(behavior_learner_t *learner) {
    learner->weight_c = 2.0;
    learner->weight_r = 1.0;
    learner->weight_n = 0.1;
    for (int i = 0; i < MAX_ACTIONS; i++) {
        learner->actions[i].action_id = (system_action_t)i;
        learner->actions[i].cumulative_score = 0.0;
        learner->actions[i].selection_count = 0;
    }
    learner->last_state.residual = 0.0;
    learner->last_state.coherence_memory = 0.0;
    learner->last_state.dimensional_state = 0.0;
}

system_action_t learner_choose_action(behavior_learner_t *learner, double current_need) {
    if (current_need < 0.2) return ACTION_SLEEP;

    // Simple epsilon-greedy for exploration (20% random)
    if ((rand() % 100) < 20) {
        // random action (excluding SLEEP)
        return (system_action_t)(rand() % 4);
    }

    system_action_t best_action = ACTION_LISTENING;
    double highest_score = -1e9;
    for (int i = 0; i < MAX_ACTIONS - 1; i++) {
        double exploration_bonus = (learner->actions[i].selection_count == 0) ? 5.0 : 0.0;
        double evaluation = learner->actions[i].cumulative_score + exploration_bonus;
        if (evaluation > highest_score) {
            highest_score = evaluation;
            best_action = learner->actions[i].action_id;
        }
    }
    return best_action;
}

void learner_evaluate_feedback(behavior_learner_t *learner, system_action_t last_action, wave_state_t current_state) {
    double delta_c = current_state.coherence_memory - learner->last_state.coherence_memory;
    double delta_r = current_state.residual - learner->last_state.residual;
    double delta_n = current_state.dimensional_state - learner->last_state.dimensional_state;

    double action_score = (learner->weight_c * delta_c)
                        - (learner->weight_r * delta_r)
                        - (learner->weight_n * delta_n);

    learner->actions[last_action].cumulative_score =
        (0.8 * learner->actions[last_action].cumulative_score) + (0.2 * action_score);
    learner->actions[last_action].selection_count++;

    learner->last_state = current_state;
}
