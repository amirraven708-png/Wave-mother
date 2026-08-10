#include "wave_mother_bridge.h"
#include <stdio.h>
#include <math.h>

void wave_mother_init(WaveMotherLink *link, Blockchain *local_bc, CrossChainGateway *gw, const char *network_id) {
    strncpy(link->mother_network_id, network_id, 63);
    link->mother_network_id[63] = '\0';
    link->local_chain = local_bc;
    link->local_gateway = gw;
    
    link->resonance_state.local_focus_amplitude = local_bc->consensus_node.wave.amplitude;
    link->resonance_state.universal_focus_phase = PI;
    link->resonance_state.memory_eccentricity = 0.0;
    
    printf("[Wave Mother] Node successfully attached to ecosystem '%s'.\n", link->mother_network_id);
}

void wave_mother_sync(WaveMotherLink *link) {
    Node *node = &link->local_chain->consensus_node;
    
    double phase_diff = fabs(node->wave.phase - link->resonance_state.universal_focus_phase);
    link->resonance_state.memory_eccentricity = sin(phase_diff / 2.0);
    
    if (link->resonance_state.memory_eccentricity > 0.5) {
        printf("[Wave Mother] High memory eccentricity (%.3f) detected. Applying phase correction...\n", 
               link->resonance_state.memory_eccentricity);
        node->wave.phase = (node->wave.phase + link->resonance_state.universal_focus_phase) / 2.0;
    }
}

void wave_mother_broadcast_dissonance(WaveMotherLink *link) {
    double current_dissonance = link->local_gateway->dissonance;
    if (current_dissonance > 0.0) {
        printf("[Wave Mother] Telemetry: Broadcasting gateway dissonance (%.4f) to Universal Mind.\n", current_dissonance);
    }
}
