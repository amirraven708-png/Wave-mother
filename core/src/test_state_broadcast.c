#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "wave_psi_core_v2.h"
#include "dimn_mesh.h"
#include "radio_port.h"

int main() {
    psi_core_engine_t psi;
    psi_core_init(&psi);

    // Initialize a radio port (dummy)
    radio_t radio;
    radio_init(&radio, 9099);
    radio_reg(&radio, 0xBBBB2222, "127.0.0.1", 9098);

    printf("=== PSI-CORE State Broadcasting ===\n");
    double t = 0.0;
    for (int step = 0; step < 20; step++) {
        // Simulated raw metrics
        double raw_residual = 85.0 * exp(-0.5*step) + 2.0*sin(step*0.3);
        double raw_coherence = 0.2 + 0.8*(1.0 - exp(-0.3*step)) + 0.05*cos(step*0.7);
        if (raw_coherence > 1.0) raw_coherence = 1.0;
        if (raw_coherence < 0.0) raw_coherence = 0.0;

        psi_core_breathe(&psi, raw_residual, raw_coherence, 0.1);
        t += 0.1;

        // Print local state
        printf("[t=%.1f] N=%.2f | C_m=%.3f | R=%.2f\n",
               t, psi.state.dimensional_state,
               psi.state.coherence_memory, psi.state.residual);

        // Broadcast state vector via DIMN
        dimn_pkt pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.magic = 0x57415645;
        pkt.type = MSG_STATE_SYNC;
        pkt.sender = 0xAAAA1111;
        pkt.rhythm = 0;
        pkt.phase = psi.state.coherence_memory * 360.0; // map coherence to phase for visualization
        pkt.amp = psi.state.dimensional_state / MAX_DEGREES_OF_FREEDOM;
        pkt.val = (uint64_t)(psi.state.residual * 100);
        // pack state into data field
        memcpy(pkt.data, &psi.state, sizeof(wave_state_t));

        // Send to peer
        radio_cast(&radio, MSG_STATE_SYNC, 0xAAAA1111, 0, pkt.phase, pkt.amp, pkt.val, (char*)&psi.state);
        usleep(100000);
    }

    // At the end, check if the network (simulated) reached consensus
    printf("\nFinal coherence memory: %.4f\n", psi.state.coherence_memory);
    if (psi.state.coherence_memory > 0.85)
        printf("✅ Harmonic consensus achieved. The ellipse is stable.\n");
    else
        printf("⏳ Still synchronizing...\n");

    return 0;
}
