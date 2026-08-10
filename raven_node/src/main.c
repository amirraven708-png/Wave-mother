#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "blockchain.h"
#include "crosschain.h"
#include "wave_mother_bridge.h"

int main(void) {
    printf("==========================================\n");
    printf(" RAVEN STARS Ω - WAVE MINING ENGINE V1.0  \n");
    printf("==========================================\n\n");

    Blockchain bc;
    blockchain_init(&bc, "primary_node");

    CrossChainGateway gateway;
    gateway_init(&gateway, &bc);

    WaveMotherLink mother_link;
    wave_mother_init(&mother_link, &bc, &gateway, "UNIVERSAL_MIND_V1");

    blockchain_add_transaction(&bc, "Alice", "Bob", 5.0, false, NULL, 0);

    CrossChainDeposit deposit;
    deposit.source_chain_id = 1;
    strcpy(deposit.tx_hash, "0xabcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
    deposit.asset_value_usd = 1000.0;
    deposit.timestamp = time(NULL);
    gateway_process_deposit(&gateway, &deposit);

    wave_mother_sync(&mother_link);
    wave_mother_broadcast_dissonance(&mother_link);

    printf("\nAttempting to mine block...\n");
    blockchain_mine_block(&bc);

    const char *proof = gateway_generate_unlock_proof(&bc.consensus_node);
    if (proof) {
        printf("Unlock proof generated: %s\n", proof);
    } else {
        printf("Consensus not strong enough for unlock.\n");
    }

    blockchain_save(&bc, "chain.dat");
    printf("Chain saved. Blocks: %d\n", bc.num_blocks);

    blockchain_free(&bc);
    return 0;
}
