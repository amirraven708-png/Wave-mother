#ifndef WAVE_ENGINE_H
#define WAVE_ENGINE_H

#include "common.h"

typedef struct {
    double amplitude;
    double frequency;
    double phase;
} WaveParams;

typedef struct {
    long long height;
    char q_sig[17];
    double chsh_violation;
    double pulse_energy;
} BlockClock;

double compute_plv(double freq1, double phase1, double freq2, double phase2, int len);
double compute_complexity_cost(double node_L, double net_avg_L);
void generate_q_signature(BlockClock *clock, double plv, double L);
void tick_block_clock(BlockClock *clock, double complexity_cost, double fx_volatility);

#endif
