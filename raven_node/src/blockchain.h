#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include "common.h"
#include "node.h"

typedef struct {
    char sender[ADDRESS_LEN];
    char recipient[ADDRESS_LEN];
    double amount;
    bool is_external;
    char external_tx_hash[65];
    uint8_t source_chain_id;
} Transaction;

typedef struct {
    int index;
    time_t timestamp;
    Transaction *transactions;
    int num_transactions;
    char previous_hash[HASH_HEX_LEN];
    char block_hash[HASH_HEX_LEN];
    int star_path[STAR_PATH_LEN];
    double block_clock;
    double difficulty;
} Block;

typedef struct {
    Block *chain;
    int num_blocks;
    Transaction *pending;
    int pending_count;
    double difficulty;
    double mining_reward;
    double expected_block_time;
    int difficulty_adjust_interval;
    Node consensus_node;
    void *gateway;
} Blockchain;

void blockchain_init(Blockchain *bc, const char *node_id);
void blockchain_free(Blockchain *bc);
void blockchain_add_transaction(Blockchain *bc, const char *from, const char *to,
                                double amount, bool external, const char *ext_tx_hash, uint8_t chain_id);
void blockchain_mine_block(Blockchain *bc);
bool blockchain_validate(Blockchain *bc);
int blockchain_save(Blockchain *bc, const char *path);
int blockchain_load(Blockchain *bc, const char *path);

#endif
