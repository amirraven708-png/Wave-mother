#ifndef WAVE_PSI_CORE_H
#define WAVE_PSI_CORE_H

#define MIN_DEGREES_OF_FREEDOM  3.0
#define MAX_DEGREES_OF_FREEDOM  1024.0

typedef struct {
    double current_dim;      // N(t)
    double alpha;            // Expansion coefficient
    double beta;             // Contraction coefficient
    double prev_residual;    // R(t-1)
} psi_core_engine_t;

void psi_core_init(psi_core_engine_t *engine);
void psi_core_breathe(psi_core_engine_t *engine, double current_residual, double dt);
double psi_core_extract_harmony(psi_core_engine_t *engine);
#endif
