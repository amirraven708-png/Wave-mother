#ifndef WAVE_GRAPH_H
#define WAVE_GRAPH_H
#include <stdint.h>
#include "wave_trace.h"

typedef enum {
    STATE_DORMANT      = 0,
    STATE_EXPLORATION  = 1,
    STATE_CONSOLIDATION = 2,
    STATE_PRODUCTION   = 3
} stability_state_t;

typedef struct {
    double FI, RI, DI, SI;
} meaning_indices_t;

typedef struct graph_node_t {
    uint64_t            node_id;
    wave_trace_t        *trace;
    stability_state_t   state;
    meaning_indices_t   indices;
    double              psi_score;
    int                 is_golden;
    size_t              edge_count, edge_capacity;
    struct graph_node_t **edges;
} graph_node_t;

typedef struct {
    graph_node_t **nodes;
    size_t       node_count, node_capacity;
    double       theta_FI, theta_RI, theta_DI, theta_SI;
    double       w_FI, w_RI, w_DI, w_SI;
} wave_graph_t;

int  wave_graph_init(wave_graph_t *g, size_t initial_capacity);
void wave_graph_free(wave_graph_t *g);
graph_node_t* wave_graph_insert(wave_graph_t *g, wave_trace_t *trace);
void wave_graph_update_indices(wave_graph_t *g, graph_node_t *node);
void wave_graph_detect_golden(wave_graph_t *g);
size_t wave_graph_freeze(graph_node_t *start, graph_node_t **path, size_t max_len);
size_t wave_graph_select(wave_graph_t *g, uint64_t rhythm_mask, uint64_t rhythm_val,
                         graph_node_t **results, size_t max_results);
#endif
