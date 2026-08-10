#include "crosschain.h"
#include <string.h>
#include <math.h>

void gateway_init(CrossChainGateway *gw, Blockchain *bc) {
    gw->total_teleported_value = 0.0;
    gw->dissonance = 0.0;
    gw->chain_ref = bc;
    bc->gateway = gw;
}

double kernel_inversion_phase(const char *tx_hash) {
    double phase = 0.0;
    for (int i = 0; i < strlen(tx_hash); i++) {
        phase += (double)(unsigned char)tx_hash[i] * 0.01;
    }
    return fmod(phase, 2 * PI);
}

void gateway_process_deposit(CrossChainGateway *gw, CrossChainDeposit *dep) {
    Blockchain *bc = gw->chain_ref;
    Node *node = &bc->consensus_node;

    double energy = dep->asset_value_usd * 0.001;
    node->wave.amplitude += energy;

    double base_phase;
    switch (dep->source_chain_id) {
        case 1: base_phase = PI/2; break;
        case 2: base_phase = PI;   break;
        case 3: base_phase = 3*PI/2; break;
        default: base_phase = 0;
    }
    node->wave.phase = fmod(base_phase + kernel_inversion_phase(dep->tx_hash), 2*PI);

    node->L += 0.05;
    if (node->L > 1.5) node->L = 1.5;

    char from[ADDRESS_LEN] = "bridge";
    char to[ADDRESS_LEN];
    snprintf(to, ADDRESS_LEN, "ext_user_%s", dep->tx_hash + 60);
    blockchain_add_transaction(bc, from, to, dep->asset_value_usd, true, dep->tx_hash, dep->source_chain_id);

    gw->total_teleported_value += dep->asset_value_usd;
    gw->dissonance += 0.01;
    printf("Cross-chain deposit: %.2f USD, new L=%.3f\n", dep->asset_value_usd, node->L);
}

const char* gateway_generate_unlock_proof(Node *node) {
    if (node->plv > 0.98) return node->clock.q_sig;
    return NULL;
}
