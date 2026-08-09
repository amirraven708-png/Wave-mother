#include "psi_optimizer_mod.h"
void psi_optimizer_init(psi_optimizer_t *opt) { if(opt){ opt->historical_success_rate=0.0; opt->target_resilience=0.95; } }
void psi_tune_parameters(psi_core_engine_t *engine, const psi_optimizer_t *opt, double average_resilience) {
    if(!engine||!opt) return;
    double err=opt->target_resilience-average_resilience;
    if(err>0.05) engine->alpha_base+=0.1;
    else if(err<-0.05) { engine->alpha_base-=0.05; if(engine->alpha_base<1.0) engine->alpha_base=1.0; }
}
