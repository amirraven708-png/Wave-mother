#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "wave_graph.h"
#include "wave_index.h"   // استفاده از ایندکس برای یافتن نامزدها

static double euler_phase_diff(uint64_t p1, uint64_t p2) {
    double a = (double)p1 / 10000.0 * 2.0 * M_PI;
    double b = (double)p2 / 10000.0 * 2.0 * M_PI;
    return cos(a - b);
}

static int dirac_shock(const wave_trace_t *t1, const wave_trace_t *t2) {
    if (!t1->content || !t2->content) return 0;
    if (t1->size != t2->size) return 1;
    int v1, v2;
    memcpy(&v1, t1->content, sizeof(int));
    memcpy(&v2, t2->content, sizeof(int));
    return abs(v1 - v2) > 500;
}

int wave_graph_init(wave_graph_t *g, size_t init_cap) {
    g->nodes = calloc(init_cap, sizeof(graph_node_t*));
    if (!g->nodes) return -1;
    g->node_count = 0;
    g->node_capacity = init_cap;
    g->theta_FI = g->theta_RI = g->theta_DI = g->theta_SI = 0.0; // تطبیقی خواهند شد
    g->w_FI = g->w_RI = g->w_DI = g->w_SI = 0.25;
    return 0;
}

void wave_graph_free(wave_graph_t *g) {
    for (size_t i = 0; i < g->node_count; i++) {
        free(g->nodes[i]->edges);
        free(g->nodes[i]);
    }
    free(g->nodes);
}

graph_node_t* wave_graph_insert(wave_graph_t *g, wave_trace_t *trace) {
    if (g->node_count >= g->node_capacity) {
        g->node_capacity *= 2;
        g->nodes = realloc(g->nodes, g->node_capacity * sizeof(graph_node_t*));
    }
    graph_node_t *n = calloc(1, sizeof(graph_node_t));
    n->node_id = trace->rhythm;
    n->trace = trace;
    n->state = STATE_EXPLORATION;
    n->edge_capacity = 4;
    n->edges = malloc(4 * sizeof(graph_node_t*));

    // فقط گره‌های با همان ۸ بیت بالای rhythm را بررسی کن
    uint64_t my_key = (trace->rhythm >> 56) & 0xFF;
    for (size_t i = 0; i < g->node_count; i++) {
        graph_node_t *o = g->nodes[i];
        uint64_t o_key = (o->trace->rhythm >> 56) & 0xFF;
        if (my_key != o_key) continue;  // رد شدن سریع

        double corr = euler_phase_diff(trace->phase, o->trace->phase);
        int shock = dirac_shock(trace, o->trace);
        if (corr > 0.7 || shock) {
            if (n->edge_count >= n->edge_capacity) {
                n->edge_capacity *= 2;
                n->edges = realloc(n->edges, n->edge_capacity * sizeof(graph_node_t*));
            }
            n->edges[n->edge_count++] = o;
            if (o->edge_count >= o->edge_capacity) {
                o->edge_capacity *= 2;
                o->edges = realloc(o->edges, o->edge_capacity * sizeof(graph_node_t*));
            }
            o->edges[o->edge_count++] = n;
        }
    }
    g->nodes[g->node_count++] = n;
    return n;
}

void wave_graph_update_indices(wave_graph_t *g, graph_node_t *n) {
    if (!n) return;
    if (n->edge_count == 0) {
        n->indices.FI = 0.0; n->indices.RI = 0.0;
        n->indices.DI = 0.0; n->indices.SI = 0.0;
        n->psi_score = 0.0;
        return;
    }
    double FI = (double)n->edge_count / g->node_count;
    uint64_t base = n->trace->rhythm & 0xF000000000000000ULL;
    size_t same = 0, self = 0;
    for (size_t i = 0; i < n->edge_count; i++) {
        if ((n->edges[i]->trace->rhythm & 0xF000000000000000ULL) == base) same++;
        if (euler_phase_diff(n->trace->phase, n->edges[i]->trace->phase) > 0.9) self++;
    }
    double RI = (double)same / n->edge_count;
    double DI = log2(n->edge_count + 1.0) / 8.0;
    double SI = (double)self / n->edge_count;
    n->indices.FI = FI; n->indices.RI = RI;
    n->indices.DI = DI; n->indices.SI = SI;
    n->psi_score = g->w_FI*FI + g->w_RI*RI + g->w_DI*DI + g->w_SI*SI;
}

void wave_graph_detect_golden(wave_graph_t *g) {
    // محاسبه میانگین و انحراف معیار برای هر شاخص
    double sum_FI=0, sum_RI=0, sum_DI=0, sum_SI=0;
    double sq_FI=0, sq_RI=0, sq_DI=0, sq_SI=0;
    for (size_t i = 0; i < g->node_count; i++) {
        graph_node_t *n = g->nodes[i];
        wave_graph_update_indices(g, n);
        sum_FI += n->indices.FI; sq_FI += n->indices.FI * n->indices.FI;
        sum_RI += n->indices.RI; sq_RI += n->indices.RI * n->indices.RI;
        sum_DI += n->indices.DI; sq_DI += n->indices.DI * n->indices.DI;
        sum_SI += n->indices.SI; sq_SI += n->indices.SI * n->indices.SI;
    }
    double mean_FI = sum_FI / g->node_count;
    double mean_RI = sum_RI / g->node_count;
    double mean_DI = sum_DI / g->node_count;
    double mean_SI = sum_SI / g->node_count;
    double std_FI = sqrt(sq_FI/g->node_count - mean_FI*mean_FI);
    double std_RI = sqrt(sq_RI/g->node_count - mean_RI*mean_RI);
    double std_DI = sqrt(sq_DI/g->node_count - mean_DI*mean_DI);
    double std_SI = sqrt(sq_SI/g->node_count - mean_SI*mean_SI);
    // آستانه تطبیقی: یک انحراف معیار بالای میانگین
    g->theta_FI = mean_FI + std_FI;
    g->theta_RI = mean_RI + std_RI;
    g->theta_DI = mean_DI + std_DI;
    g->theta_SI = mean_SI + std_SI;

    for (size_t i = 0; i < g->node_count; i++) {
        graph_node_t *n = g->nodes[i];
        n->is_golden = (n->indices.FI > g->theta_FI && n->indices.RI > g->theta_RI &&
                        n->indices.DI > g->theta_DI && n->indices.SI > g->theta_SI);
        if (n->is_golden) n->state = STATE_PRODUCTION;
    }
}

size_t wave_graph_freeze(graph_node_t *start, graph_node_t **path, size_t max_len) {
    size_t len = 0;
    graph_node_t *cur = start;
    while (len < max_len && cur) {
        path[len++] = cur;
        graph_node_t *best = NULL;
        double best_psi = -1.0;
        for (size_t i = 0; i < cur->edge_count; i++) {
            graph_node_t *nb = cur->edges[i];
            int seen = 0;
            for (size_t j = 0; j < len; j++) if (path[j] == nb) { seen = 1; break; }
            if (!seen && nb->psi_score > best_psi) { best_psi = nb->psi_score; best = nb; }
        }
        cur = best;
    }
    return len;
}

size_t wave_graph_select(wave_graph_t *g, uint64_t mask, uint64_t val,
                         graph_node_t **res, size_t max_res) {
    size_t cnt = 0;
    for (size_t i = 0; i < g->node_count && cnt < max_res; i++) {
        if ((g->nodes[i]->trace->rhythm & mask) == (val & mask))
            res[cnt++] = g->nodes[i];
    }
    return cnt;
}
