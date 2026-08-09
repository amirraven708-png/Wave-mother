#include <stdio.h>
#include <math.h>
#include "wave_behavior.h"
#include "wave_psi_core_v2.h"

int main() {
    behavior_engine_t be;
    behavior_init(&be);

    psi_core_engine_t psi;
    psi_core_init(&psi);

    printf("=== REACTIVE / PROACTIVE BEHAVIOR DEMO ===\n");
    double t = 0.0;

    // Simulate a network that has waves of need
    double need_pattern[] = {0.1, 0.2, 0.5, 0.7, 0.6, 0.3, 0.1, 0.05, 0.0, 0.0, 0.1, 0.4, 0.8, 0.9, 0.5, 0.2};

    for (int i = 0; i < 16; i++) {
        double need = need_pattern[i];
        double load = 0.3;  // stable load
        double cap  = 0.8;  // high capacity available

        behavior_update(&be, load, need, cap, 75.0, 1.0);

        psi_core_breathe(&psi, need * 50.0, be.network_need_level, 1.0);

        printf("[t=%.0f] Need=%.2f | State=%-20s | Heartbeat=%.1f° | N=%.1f | Action: %s\n",
               t, need, behavior_state_name(be.state), be.heartbeat_phase,
               psi.state.dimensional_state,
               behavior_is_proactive(&be) ? "GENERATING WAVES" : "LISTENING");

        t += 1.0;
    }

    printf("\n✅ Behavior engine preserved heartbeat: %.1f°\n", be.heartbeat_phase);
    return 0;
}
