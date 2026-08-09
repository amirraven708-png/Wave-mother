#ifndef PSI_OPTIMIZER_MOD_H
#define PSI_OPTIMIZER_MOD_H
#include "wave_psi_core_v2.h"
typedef struct { double historical_success_rate; double target_resilience; } psi_optimizer_t;
void psi_optimizer_init(psi_optimizer_t *opt);
void psi_tune_parameters(psi_core_engine_t *engine, const psi_optimizer_t *opt, double average_resilience);
#endif
