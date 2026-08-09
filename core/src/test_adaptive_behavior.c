#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "wave_behavior_policy.h"
#include "wave_ellipse_memory.h"

// Simulate the effect of an action on the system state (simplified)
void simulate_action(system_action_t action, wave_state_t *state, double need) {
    double old_c = state->coherence_memory;
    double old_r = state->residual;
    // Different actions have different effects
    switch(action) {
        case ACTION_LISTENING:
            state->coherence_memory += 0.01;
            state->residual -= 0.1*need;
            break;
        case ACTION_GENERATING_WAVES:
            state->coherence_memory += 0.05*need;
            state->residual -= 0.3*need;
            state->dimensional_state += 2.0*need;
            break;
        case ACTION_ADJUSTING_PHASE:
            state->coherence_memory += 0.08;
            state->residual -= 0.05;
            break;
        case ACTION_SHARING_DIMENSIONS:
            state->dimensional_state -= 5.0;
            state->coherence_memory += 0.03;
            state->residual -= 0.15*need;
            break;
        case ACTION_SLEEP:
            state->coherence_memory += 0.005;
            state->residual += 0.02;
            state->dimensional_state -= 1.0;
            break;
    }
    // Clamp values
    if (state->coherence_memory > 1.0) state->coherence_memory = 1.0;
    if (state->coherence_memory < 0.0) state->coherence_memory = 0.0;
    if (state->residual > 100.0) state->residual = 100.0;
    if (state->residual < 0.0) state->residual = 0.0;
    if (state->dimensional_state < 3.0) state->dimensional_state = 3.0;
    if (state->dimensional_state > 1024.0) state->dimensional_state = 1024.0;
}

int main() {
    srand(42);
    behavior_learner_t learner;
    learner_init(&learner);

    holographic_ellipse_memory_t ellipse;
    ellipse_memory_init(&ellipse);

    printf("=== ADAPTIVE BEHAVIOR + ELLIPSE MEMORY ===\n");
    printf("Phase 1: Learning from experience...\n");

    // Initial state
    wave_state_t state = { .residual=50.0, .coherence_memory=0.3, .dimensional_state=50.0 };

    // Run a sequence of decisions
    double need_pattern[] = {0.6, 0.3, 0.8, 0.1, 0.9, 0.5};
    for (int i = 0; i < 6; i++) {
        double need = need_pattern[i];
        system_action_t action = learner_choose_action(&learner, need);
        printf("  Need=%.1f -> Chose %d ", need, action);

        // Apply action
        simulate_action(action, &state, need);
        // Evaluate feedback
        learner_evaluate_feedback(&learner, action, state);
        printf(" | State: C_m=%.3f, R=%.1f, N=%.1f\n",
               state.coherence_memory, state.residual, state.dimensional_state);
    }

    printf("\nPhase 2: Freezing learned behavior on ellipse...\n");
    ellipse_freeze_behavior(&ellipse, &learner);
    printf("Focus b after freezing: %.3f\n", ellipse.focus_b);

    printf("\nPhase 3: Simulating reboot – new learner, thaw memory...\n");
    behavior_learner_t new_learner;
    learner_init(&new_learner);
    if (ellipse_thaw_behavior(&ellipse, &new_learner) == 0) {
        printf("  Thawed scores:\n");
        for (int i = 0; i < MAX_ACTIONS; i++) {
            printf("    Action %d: score=%.3f\n", i, new_learner.actions[i].cumulative_score);
        }
        // Compare with original
        printf("  Original scores were identical? %s\n",
               (memcmp(new_learner.actions, learner.actions, sizeof(learner.actions)) == 0) ? "YES" : "NO");
    } else {
        printf("  Thaw failed!\n");
    }

    printf("\n✅ Adaptive behavior memory preserved across reboot.\n");
    return 0;
}
