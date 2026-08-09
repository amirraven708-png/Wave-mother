#include <stdio.h>
#include "wave_scheduler.h"

int main() {
    node_candidate_t candidates[] = {
        { .node_id = 0xAAAA1111, .available_capacity = 500, .reputation = 80.0, .phase = 45.0,  .latency_ms = 10.0 },
        { .node_id = 0xBBBB2222, .available_capacity = 900, .reputation = 95.0, .phase = 100.0, .latency_ms = 5.0  },
        { .node_id = 0xCCCC3333, .available_capacity = 200, .reputation = 60.0, .phase = 170.0, .latency_ms = 50.0 },
        { .node_id = 0xDDDD4444, .available_capacity = 800, .reputation = 70.0, .phase = 10.0,  .latency_ms = 200.0 }, // تأخیر بالا
    };

    double local_phase = 90.0; // فاز نود درخواست‌کننده

    int best = wave_scheduler_select(candidates, 4, local_phase);
    if (best >= 0) {
        printf("Best node: 0x%lX (score calculated internally)\n", candidates[best].node_id);
        printf("  Capacity: %lu, Reputation: %.1f, Phase: %.1f°, Latency: %.1fms\n",
               candidates[best].available_capacity,
               candidates[best].reputation,
               candidates[best].phase,
               candidates[best].latency_ms);
    } else {
        printf("No suitable node found.\n");
    }

    return 0;
}
