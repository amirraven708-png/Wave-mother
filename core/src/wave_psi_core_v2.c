#include <math.h>
#include "wave_psi_core_v2.h"
void psi_core_init(psi_core_engine_t *e) {
    e->dimensional_state=16.0; e->alpha_base=1.25; e->beta_base=0.45; e->gamma=0.20;
    e->prev_residual=1.0; e->prev_coherence=0.0; e->coherence=0.0; e->dimensional_velocity=0.0;
}
void psi_core_breathe(psi_core_engine_t *e, double residual, double coherence, double dt) {
    if(residual<0)residual=0; if(residual>1)residual=1;
    if(coherence<0)coherence=0; if(coherence>1)coherence=1;
    double rp=e->prev_residual-residual, pf=(rp>0)?1.0:0.2;
    double da=e->alpha_base*(1.0-coherence)*pf;
    double db=e->beta_base*(0.5+coherence)*(1.0+(1.0-residual));
    double vel=da*residual - db*(e->dimensional_state-MIN_DEGREES_OF_FREEDOM) + e->gamma*coherence;
    if(vel>12)vel=12; if(vel<-12)vel=-12;
    e->dimensional_velocity=vel; e->dimensional_state+=vel*dt;
    if(e->dimensional_state<MIN_DEGREES_OF_FREEDOM) e->dimensional_state=MIN_DEGREES_OF_FREEDOM;
    if(e->dimensional_state>MAX_DEGREES_OF_FREEDOM) e->dimensional_state=MAX_DEGREES_OF_FREEDOM;
    e->prev_residual=residual; e->prev_coherence=e->coherence; e->coherence=coherence;
}
