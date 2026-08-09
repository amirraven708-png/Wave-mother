#include <stdio.h>
#include <math.h>
#include "wave_psi_core.h"

void psi_core_init(psi_core_engine_t *engine) {
    engine->current_dim = MIN_DEGREES_OF_FREEDOM;
    engine->alpha = 2.5;
    engine->beta = 0.8;
    engine->prev_residual = 0.0;
}

void psi_core_breathe(psi_core_engine_t *engine, double current_residual, double dt) {
    double delta_n = (engine->alpha * current_residual) - 
                     (engine->beta * (engine->current_dim - MIN_DEGREES_OF_FREEDOM));
    engine->current_dim += delta_n * dt;
    if (engine->current_dim < MIN_DEGREES_OF_FREEDOM) engine->current_dim = MIN_DEGREES_OF_FREEDOM;
    if (engine->current_dim > MAX_DEGREES_OF_FREEDOM) engine->current_dim = MAX_DEGREES_OF_FREEDOM;
    engine->prev_residual = current_residual;
}

double psi_core_extract_harmony(psi_core_engine_t *engine) {
    return (engine->current_dim <= MIN_DEGREES_OF_FREEDOM + 0.1) ? 1.0 : 0.0;
}
