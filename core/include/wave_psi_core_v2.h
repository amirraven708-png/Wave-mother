#ifndef WAVE_PSI_CORE_V2_H
#define WAVE_PSI_CORE_V2_H
#define MIN_DEGREES_OF_FREEDOM  3.0
#define MAX_DEGREES_OF_FREEDOM  200.0
typedef struct {
    double dimensional_state, alpha_base, beta_base, gamma;
    double prev_residual, prev_coherence, coherence, dimensional_velocity;
} psi_core_engine_t;
void psi_core_init(psi_core_engine_t *e);
void psi_core_breathe(psi_core_engine_t *e, double residual, double coherence, double dt);
#endif
