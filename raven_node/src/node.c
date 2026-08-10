#include "node.h"
#include <stdio.h>
#include <string.h>

static double simple_rand() {
    static unsigned long state = 12345;
    state = state * 1103515245 + 12345;
    return (double)((state / 65536) % 32768) / 32768.0;
}

void node_init(Node *node, const char *id, double base_L) {
    strncpy(node->id, id, 15);
    node->wave.amplitude = 1.0;
    node->wave.frequency = 1.0;
    node->wave.phase = simple_rand() * 2 * PI;
    node->L = base_L;
    node->plv = 0.0;
    node->complexity_cost = 0.0;
    node->clock.height = 0;
    node->clock.chsh_violation = 0.0;
    node->clock.pulse_energy = 0.0;
    memset(node->clock.q_sig, 0, sizeof(node->clock.q_sig));
    node->balances.count = 0;
    snprintf(node->address, ADDRESS_LEN, "miner_%s", id);
}

void node_update_wave_buffers(Node *node, double *buffer, int len) {
    for (int i = 0; i < len; i++) {
        double t = (double)i / (len - 1);
        buffer[i] = node->wave.amplitude * sin(2 * PI * node->wave.frequency * t + node->wave.phase);
    }
}

void node_inject_fx(Node *node, double fx_volatility, double fx_trend) {
    node->L = 0.5 + fx_volatility * 2.0 + fx_trend * 0.5;
    if (node->L > 1.5) node->L = 1.5;
    if (node->L < 0.1) node->L = 0.1;
    node->wave.frequency = 1.0 + fx_volatility * 2.0;
    node->wave.phase += fx_volatility * 0.2;
}

void node_compute_consensus(Node *node, double target_freq, double target_phase,
                            int wave_len, double net_avg_L) {
    node->plv = compute_plv(node->wave.frequency, node->wave.phase,
                             target_freq, target_phase, wave_len);
    node->complexity_cost = compute_complexity_cost(node->L, net_avg_L);
    double vol = (node->L - 0.5) / 2.0;
    tick_block_clock(&node->clock, node->complexity_cost, vol);
    generate_q_signature(&node->clock, node->plv, node->L);
}

void node_apply_transaction(Node *node, const char *from, const char *to, double amount, bool is_external) {
    for (int i = 0; i < node->balances.count; i++) {
        BalanceEntry *e = &node->balances.entries[i];
        if (strcmp(e->address, from) == 0) {
            if (is_external) e->balance_ext -= amount;
            else e->balance_rvc -= amount;
        }
        if (strcmp(e->address, to) == 0) {
            if (is_external) e->balance_ext += amount;
            else e->balance_rvc += amount;
        }
    }
}

double node_get_balance(Node *node, const char *address, bool external) {
    for (int i = 0; i < node->balances.count; i++) {
        if (strcmp(node->balances.entries[i].address, address) == 0) {
            return external ? node->balances.entries[i].balance_ext
                            : node->balances.entries[i].balance_rvc;
        }
    }
    return 0.0;
}
