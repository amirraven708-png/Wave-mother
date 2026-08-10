#ifndef CROSSCHAIN_H
#define CROSSCHAIN_H

#include "common.h"
#include "node.h"
#include "blockchain.h"

typedef struct {
    uint8_t source_chain_id;
    char tx_hash[65];
    double asset_value_usd;
    uint64_t timestamp;
} CrossChainDeposit;

typedef struct {
    double total_teleported_value;
    double dissonance;
    Blockchain *chain_ref;
} CrossChainGateway;

void gateway_init(CrossChainGateway *gw, Blockchain *bc);
void gateway_process_deposit(CrossChainGateway *gw, CrossChainDeposit *deposit);
const char* gateway_generate_unlock_proof(Node *node);

#endif
