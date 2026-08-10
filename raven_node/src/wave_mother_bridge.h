#ifndef WAVE_MOTHER_BRIDGE_H
#define WAVE_MOTHER_BRIDGE_H

#include "common.h"
#include "blockchain.h"
#include "crosschain.h"
#include "wave_engine.h"

typedef struct {
    double local_focus_amplitude;
    double universal_focus_phase;
    double memory_eccentricity;
} EllipticalMemoryState;

typedef struct {
    char mother_network_id[64];
    EllipticalMemoryState resonance_state;
    Blockchain *local_chain;
    CrossChainGateway *local_gateway;
} WaveMotherLink;

void wave_mother_init(WaveMotherLink *link, Blockchain *local_bc, CrossChainGateway *gw, const char *network_id);
void wave_mother_sync(WaveMotherLink *link);
void wave_mother_broadcast_dissonance(WaveMotherLink *link);

#endif
