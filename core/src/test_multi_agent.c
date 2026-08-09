#include <stdio.h>
#include <stdlib.h>
#include "wave_multi_agent.h"

int main() {
    srand(12345);  // reproducibility

    multi_agent_system_t sys;
    mas_init(&sys, 0.8);  // goal coherence 0.8

    double initial_coh = 0.3;
    double initial_res = 0.0;

    // Run 6 stages, each stage add 2 agents
    for (int stage = 0; stage < 6; stage++) {
        mas_add_two_agents(&sys);
        mas_run_stage(&sys, initial_coh, initial_res);
        // Update initial coherence to a mix (simulate system learning)
        initial_coh = initial_coh * 0.8 + 0.2 * 0.5; // drift towards 0.5
    }

    // Select best combined path
    agent_decision_t path[MAX_COMBINED_PATH];
    int path_len;
    mas_select_best_combined_path(&sys, path, &path_len);

    printf("=== MULTI-AGENT PATH LEARNER RESULTS ===\n");
    printf("Agents created: %d\n", sys.agent_count);
    printf("Stages completed: %d\n", sys.stage_count);
    printf("Total resource used: %.2f\n", sys.total_resource_used);
    printf("Best combined path (length %d):\n", path_len);
    for (int i = 0; i < path_len; i++) {
        printf("  Stage %d: Decision %s\n", i+1, path[i] == DECISION_A ? "A" : "B");
    }
    double efficiency = mas_calculate_path_efficiency(&sys, path, path_len);
    printf("Path efficiency (coherence gain / resource): %.4f\n", efficiency);
    printf("============================================\n");

    return 0;
}
