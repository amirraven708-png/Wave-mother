#include "blockchain.h"
#include "wave_engine.h"
#include "crypto_utils.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

void blockchain_init(Blockchain *bc, const char *node_id) {
    bc->chain = NULL;
    bc->num_blocks = 0;
    bc->pending = malloc(MAX_PENDING_TX * sizeof(Transaction));
    bc->pending_count = 0;
    bc->difficulty = 1.0;
    bc->mining_reward = 10.0;
    bc->expected_block_time = 60.0;
    bc->difficulty_adjust_interval = 10;
    bc->gateway = NULL;

    node_init(&bc->consensus_node, node_id, 0.6);

    Block genesis;
    genesis.index = 0;
    genesis.timestamp = time(NULL);
    genesis.transactions = NULL;
    genesis.num_transactions = 0;
    strcpy(genesis.previous_hash, "0");
    sha256_hex("genesis", genesis.block_hash);
    genesis.star_path[0] = 1; genesis.star_path[1] = 2; genesis.star_path[2] = 3;
    genesis.star_path[3] = 4; genesis.star_path[4] = 5;
    genesis.block_clock = 1.0;
    genesis.difficulty = bc->difficulty;
    bc->chain = malloc(sizeof(Block));
    memcpy(bc->chain, &genesis, sizeof(Block));
    bc->num_blocks = 1;
}

void blockchain_free(Blockchain *bc) {
    for (int i = 0; i < bc->num_blocks; i++) free(bc->chain[i].transactions);
    free(bc->chain);
    free(bc->pending);
}

void blockchain_add_transaction(Blockchain *bc, const char *from, const char *to,
                                double amount, bool external, const char *ext_tx_hash, uint8_t chain_id) {
    if (bc->pending_count >= MAX_PENDING_TX) return;
    Transaction *tx = &bc->pending[bc->pending_count++];
    strncpy(tx->sender, from, ADDRESS_LEN-1);
    strncpy(tx->recipient, to, ADDRESS_LEN-1);
    tx->amount = amount;
    tx->is_external = external;
    if (ext_tx_hash) strncpy(tx->external_tx_hash, ext_tx_hash, 64);
    else tx->external_tx_hash[0] = '\0';
    tx->source_chain_id = chain_id;
}

void blockchain_mine_block(Blockchain *bc) {
    double target_freq = 1.0;
    double target_phase = 0.0;
    double net_avg_L = bc->consensus_node.L;
    node_compute_consensus(&bc->consensus_node, target_freq, target_phase, 100, net_avg_L);

    if (bc->consensus_node.plv < 0.7) {
        printf("Consensus not reached (PLV %.3f), no block mined.\n", bc->consensus_node.plv);
        return;
    }

    Block *newb = malloc(sizeof(Block));
    newb->index = bc->num_blocks;
    newb->timestamp = time(NULL);
    newb->num_transactions = bc->pending_count;
    newb->transactions = malloc(bc->pending_count * sizeof(Transaction));
    memcpy(newb->transactions, bc->pending, bc->pending_count * sizeof(Transaction));
    strcpy(newb->previous_hash, bc->chain[bc->num_blocks-1].block_hash);

    for (int i = 0; i < STAR_PATH_LEN; i++)
        newb->star_path[i] = (bc->consensus_node.clock.q_sig[i] % 5) + 1;
    newb->block_clock = bc->consensus_node.clock.height;
    newb->difficulty = bc->difficulty;

    char concat[256];
    snprintf(concat, sizeof(concat), "%s%ld%s", newb->previous_hash, newb->timestamp, bc->consensus_node.clock.q_sig);
    sha256_hex(concat, newb->block_hash);

    for (int i = 0; i < newb->num_transactions; i++) {
        Transaction *tx = &newb->transactions[i];
        node_apply_transaction(&bc->consensus_node, tx->sender, tx->recipient, tx->amount, tx->is_external);
    }
    char miner_addr[ADDRESS_LEN];
    snprintf(miner_addr, ADDRESS_LEN, "miner_%s", bc->consensus_node.id);
    node_apply_transaction(&bc->consensus_node, "network", miner_addr, bc->mining_reward, false);

    bc->chain = realloc(bc->chain, (bc->num_blocks+1) * sizeof(Block));
    memcpy(&bc->chain[bc->num_blocks], newb, sizeof(Block));
    bc->num_blocks++;
    free(newb);

    bc->pending_count = 0;
    printf("Block #%d mined. PLV=%.3f, Block Clock=%lld\n",
           bc->chain[bc->num_blocks-1].index, bc->consensus_node.plv, bc->consensus_node.clock.height);
}

bool blockchain_validate(Blockchain *bc) {
    for (int i = 1; i < bc->num_blocks; i++) {
        char computed_hash[HASH_HEX_LEN];
        char concat[256];
        snprintf(concat, sizeof(concat), "%s%ld%s",
                 bc->chain[i].previous_hash, bc->chain[i].timestamp, bc->chain[i].star_path);
        sha256_hex(concat, computed_hash);
        if (strcmp(computed_hash, bc->chain[i].block_hash) != 0) return false;
    }
    return true;
}

int blockchain_save(Blockchain *bc, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(&bc->num_blocks, sizeof(int), 1, f);
    for (int i = 0; i < bc->num_blocks; i++) {
        Block *b = &bc->chain[i];
        fwrite(b, sizeof(Block), 1, f);
        fwrite(b->transactions, sizeof(Transaction), b->num_transactions, f);
    }
    fclose(f);
    return 0;
}

int blockchain_load(Blockchain *bc, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int num;
    fread(&num, sizeof(int), 1, f);
    for (int i = 0; i < num; i++) {
        Block b;
        fread(&b, sizeof(Block), 1, f);
        b.transactions = malloc(b.num_transactions * sizeof(Transaction));
        fread(b.transactions, sizeof(Transaction), b.num_transactions, f);
        bc->chain = realloc(bc->chain, (bc->num_blocks+1)*sizeof(Block));
        memcpy(&bc->chain[bc->num_blocks], &b, sizeof(Block));
        bc->num_blocks++;
    }
    fclose(f);
    return 0;
}
