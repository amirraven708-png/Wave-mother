#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "wave_behavior_policy.h"
#include "wave_ellipse_memory.h"
#include "wave_psi_core_v2.h"
#include "wave_behavior.h"

// Simulate the effect of an action on the system state
void simulate_action(system_action_t action, wave_state_t *state, double need) {
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
    // Clamp
    if (state->coherence_memory > 1.0) state->coherence_memory = 1.0;
    if (state->coherence_memory < 0.0) state->coherence_memory = 0.0;
    if (state->residual > 100.0) state->residual = 100.0;
    if (state->residual < 0.0) state->residual = 0.0;
    if (state->dimensional_state < 3.0) state->dimensional_state = 3.0;
    if (state->dimensional_state > 1024.0) state->dimensional_state = 1024.0;
}

int main() {
    srand(42);

    // 1. Initialize PSI-Core (breathing engine)
    psi_core_engine_t psi;
    psi_core_init(&psi);

    // 2. Initialize Behavior Engine (reactive/proactive)
    behavior_engine_t be;
    behavior_init(&be);

    // 3. Initialize Learner (adaptive policy)
    behavior_learner_t learner;
    learner_init(&learner);

    // 4. Initialize Ellipse Memory
    holographic_ellipse_memory_t ellipse;
    ellipse_memory_init(&ellipse);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   WAVE MOTHER – FULL INTEGRATION TEST        ║\n");
    printf("║   PSI-Core + Behavior + Learner + Ellipse    ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    // Initial state
    wave_state_t state = { .residual=50.0, .coherence_memory=0.3, .dimensional_state=50.0 };

    double need_pattern[] = {0.6, 0.3, 0.8, 0.1, 0.9, 0.5, 0.7, 0.2, 0.4, 0.85};
    double t = 0.0;

    for (int cycle = 0; cycle < 10; cycle++) {
        double need = need_pattern[cycle];
        double load = 0.3;
        double cap  = 0.8;

        // --- Update Reactive/Proactive behavior ---
        behavior_update(&be, load, need, cap, 75.0, 1.0);

        // --- Learner chooses action based on need ---
        system_action_t action = learner_choose_action(&learner, need);

        // --- Apply action to simulated state ---
        simulate_action(action, &state, need);

        // --- Update PSI-Core (breathing) with current state ---
        psi.state = state;
        psi_core_breathe(&psi, state.residual, state.coherence_memory, 1.0);

        // --- Learner evaluates feedback ---
        learner_evaluate_feedback(&learner, action, state);

        printf("[t=%.0f] Need=%.2f | Behavior=%-12s | Action=%-20s | C_m=%.3f | R=%.1f | N=%.1f | Heartbeat=%.1f°\n",
               t, need,
               behavior_state_name(be.state),
               (action==ACTION_LISTENING?"LISTENING":
                action==ACTION_GENERATING_WAVES?"GENERATING_WAVES":
                action==ACTION_ADJUSTING_PHASE?"ADJUSTING_PHASE":
                action==ACTION_SHARING_DIMENSIONS?"SHARING_DIMENSIONS":"SLEEP"),
               state.coherence_memory, state.residual, state.dimensional_state,
               be.heartbeat_phase);

        t += 1.0;
    }

    // Freeze learned behavior on ellipse
    printf("\n--- Freezing behavior on ellipse ---\n");
    ellipse_freeze_behavior(&ellipse, &learner);
    printf("Ellipse focus_b = %.3f, eccentricity = %.3f\n", ellipse.focus_b, ellipse.eccentricity);

    // Simulate reboot: thaw
    printf("\n--- Simulating reboot ---\n");
    behavior_learner_t new_learner;
    learner_init(&new_learner);
    if (ellipse_thaw_behavior(&ellipse, &new_learner) == 0) {
        printf("Thawed scores:\n");
        for (int i = 0; i < MAX_ACTIONS; i++) {
            printf("  Action %d: score=%.3f (original=%.3f)\n",
                   i, new_learner.actions[i].cumulative_score, learner.actions[i].cumulative_score);
        }
    }

    printf("\n✅ Full integration test complete.\n");
    return 0;
}
