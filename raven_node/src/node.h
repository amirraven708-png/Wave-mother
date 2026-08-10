#ifndef NODE_H
#define NODE_H

#include "common.h"
#include "wave_engine.h"

#define MAX_BALANCE_ENTRIES 100

typedef struct {
    char address[ADDRESS_LEN];
    double balance_rvc;
    double balance_ext;
} BalanceEntry;

typedef struct {
    BalanceEntry entries[MAX_BALANCE_ENTRIES];
    int count;
} BalanceMap;

typedef struct {
    char id[16];
    WaveParams wave;
    double L;
    double plv;
    double complexity_cost;
    BlockClock clock;
    BalanceMap balances;
    char address[ADDRESS_LEN];
} Node;

void node_init(Node *node, const char *id, double base_L);
void node_update_wave_buffers(Node *node, double *buffer, int len);
void node_inject_fx(Node *node, double fx_volatility, double fx_trend);
void node_compute_consensus(Node *node, double target_freq, double target_phase, int wave_len, double net_avg_L);
void node_apply_transaction(Node *node, const char *from, const char *to, double amount, bool is_external);
double node_get_balance(Node *node, const char *address, bool external);

#endif
