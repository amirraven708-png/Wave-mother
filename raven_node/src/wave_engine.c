#include "wave_engine.h"
#include "crypto_utils.h"

double compute_plv(double freq1, double phase1, double freq2, double phase2, int len) {
    double sum_cos = 0.0, sum_sin = 0.0;
    for (int i = 0; i < len; i++) {
        double t = (double)i / (len - 1);
        double theta1 = 2 * PI * freq1 * t + phase1;
        double theta2 = 2 * PI * freq2 * t + phase2;
        double diff = theta1 - theta2;
        sum_cos += cos(diff);
        sum_sin += sin(diff);
    }
    return sqrt(sum_cos*sum_cos + sum_sin*sum_sin) / len;
}

double compute_complexity_cost(double node_L, double net_avg_L) {
    double delta = fabs(node_L - net_avg_L);
    return node_L * (1.0 + delta);
}

void generate_q_signature(BlockClock *clock, double plv, double L) {
    double beta = plv * L;
    clock->chsh_violation = 2.0 * sqrt(2.0) * sin(beta);
    char raw[128];
    snprintf(raw, sizeof(raw), "%lld|%.6f|%.6f", clock->height, clock->chsh_violation, L);
    unsigned long h = djb2_hash(raw);
    snprintf(clock->q_sig, 17, "%016lx", h);
}

void tick_block_clock(BlockClock *clock, double complexity_cost, double fx_volatility) {
    long long step = 1 + (long long)(complexity_cost * 5.0);
    clock->height += step;
    clock->pulse_energy = complexity_cost * fx_volatility;
}
