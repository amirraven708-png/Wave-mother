#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"
#include "psi_optimizer_mod.h"

#define INSTANCES 300

typedef struct { int value, weight; } sol_stats_t;

static sol_stats_t evaluate(const knapsack_problem_t *kp, const int *sel) {
    sol_stats_t s = {0,0};
    for (int i = 0; i < kp->count; i++) if (sel[i]) { s.value += kp->items[i].value; s.weight += kp->items[i].weight; }
    return s;
}

static void build_greedy(const knapsack_problem_t *kp, int *sel) {
    int n = kp->count; int order[MAX_ITEMS]; double ratio[MAX_ITEMS];
    for (int i = 0; i < n; i++) { order[i] = i; ratio[i] = (double)kp->items[i].value / kp->items[i].weight; }
    for (int i = 0; i < n-1; i++) for (int j = i+1; j < n; j++)
        if (ratio[order[j]] > ratio[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    memset(sel, 0, n * sizeof(int));
    int w = 0;
    for (int i = 0; i < n; i++) {
        int idx = order[i];
        if (w + kp->items[idx].weight <= kp->capacity) { sel[idx] = 1; w += kp->items[idx].weight; }
    }
}

static int improve_1x1(const knapsack_problem_t *kp, int *sel) {
    sol_stats_t best = evaluate(kp, sel); int improved = 0;
    for (int out = 0; out < kp->count; out++) {
        if (!sel[out]) continue;
        for (int in = 0; in < kp->count; in++) {
            if (sel[in]) continue;
            int nw = best.weight - kp->items[out].weight + kp->items[in].weight;
            if (nw > kp->capacity) continue;
            int nv = best.value - kp->items[out].value + kp->items[in].value;
            if (nv > best.value) { sel[out] = 0; sel[in] = 1; best.weight = nw; best.value = nv; improved = 1; }
        }
    }
    return improved;
}

static void psi_perturb(const knapsack_problem_t *kp, int *sel, double N) {
    int attempts = (int)(N * 0.5); if (attempts < 1) attempts = 1; if (attempts > kp->count) attempts = kp->count;
    for (int k = 0; k < attempts; k++) { int idx = rand() % kp->count; sel[idx] = sel[idx] ? 0 : 1; }
    sol_stats_t s = evaluate(kp, sel);
    while (s.weight > kp->capacity) {
        int idx = rand() % kp->count; if (sel[idx]) { sel[idx] = 0; s = evaluate(kp, sel); }
    }
}

void write_csv_header(FILE *f) {
    fprintf(f, "seed,model,final_value,improvements,sum_N,resilience_score\n");
}

void write_csv_row(FILE *f, int seed, const char *model, int val, int imp, double sumN, double resil) {
    fprintf(f, "%d,%s,%d,%d,%.4f,%.4f\n", seed, model, val, imp, sumN, resil);
}

int main() {
    srand(12345);
    int n_items = 200, max_w = 50, max_v = 200, cap = 1000;

    FILE *csv = fopen("logs/psi_learning_curve.csv", "w");
    if (!csv) { perror("Failed to open CSV"); return 1; }
    write_csv_header(csv);

    double grb_sum = 0, grls_sum = 0, psi_sum = 0;
    double grb_resil = 0, grls_resil = 0, psi_resil = 0;
    double grb_imp = 0, grls_imp = 0, psi_imp = 0;
    double grb_N = 0, grls_N = 0, psi_N = 0;
    int psi_better = 0, psi_equal = 0, psi_worse = 0;

    psi_optimizer_t opt;
    psi_optimizer_init(&opt);

    for (int seed = 0; seed < INSTANCES; seed++) {
        srand(1000 + seed);
        knapsack_problem_t kp;
        knapsack_generate(&kp, n_items, max_w, max_v, cap);
        int n = kp.count;
        int sel[MAX_ITEMS];
        sol_stats_t s;

        // GR-B
        build_greedy(&kp, sel);
        s = evaluate(&kp, sel);
        int grb_val = s.value;
        grb_sum += grb_val;
        int sel_noisy[MAX_ITEMS]; memcpy(sel_noisy, sel, n*sizeof(int));
        for (int i = 0; i < 50; i++) { int idx = rand() % n; sel_noisy[idx] = sel_noisy[idx] ? 0 : 1; }
        sol_stats_t noisy_s = evaluate(&kp, sel_noisy);
        while (noisy_s.weight > kp.capacity) { int idx = rand() % n; if (sel_noisy[idx]) { sel_noisy[idx] = 0; noisy_s = evaluate(&kp, sel_noisy); } }
        double resil_grb = (noisy_s.value >= grb_val) ? 1.0 : 0.0;
        grb_resil += resil_grb;
        write_csv_row(csv, seed, "GR-B", grb_val, 0, 0.0, resil_grb);

        // GR-LS
        memcpy(sel, sel, n*sizeof(int));
        int imp_ls = 0;
        while (improve_1x1(&kp, sel)) imp_ls++;
        s = evaluate(&kp, sel);
        int grls_val = s.value;
        grls_sum += grls_val;
        grls_imp += imp_ls;
        memcpy(sel_noisy, sel, n*sizeof(int));
        for (int i = 0; i < 50; i++) { int idx = rand() % n; sel_noisy[idx] = sel_noisy[idx] ? 0 : 1; }
        noisy_s = evaluate(&kp, sel_noisy);
        while (noisy_s.weight > kp.capacity) { int idx = rand() % n; if (sel_noisy[idx]) { sel_noisy[idx] = 0; noisy_s = evaluate(&kp, sel_noisy); } }
        double resil_grls = (noisy_s.value >= grls_val) ? 1.0 : 0.0;
        grls_resil += resil_grls;
        write_csv_row(csv, seed, "GR-LS", grls_val, imp_ls, 0.0, resil_grls);

        // PSI-CORE
        psi_core_engine_t psi;
        psi_core_init(&psi);
        psi_tune_parameters(&psi, &opt, psi_resil / (seed + 1));
        double residual = 1.0, coherence = 0.3;
        int best_sel[MAX_ITEMS], cur[MAX_ITEMS];
        build_greedy(&kp, best_sel);
        s = evaluate(&kp, best_sel);
        int psi_val = s.value;
        int psi_improvements = 0;
        double psi_sum_N = 0.0;
        int no_improve = 0;
        for (int iter = 0; iter < 100; iter++) {
            psi_core_breathe(&psi, residual, coherence, 0.5);
            double N = psi.dimensional_state;
            psi_sum_N += N;
            memcpy(cur, best_sel, n*sizeof(int));
            psi_perturb(&kp, cur, N);
            int ls = 0; while (improve_1x1(&kp, cur)) ls++;
            s = evaluate(&kp, cur);
            if (s.value > psi_val) {
                psi_val = s.value;
                memcpy(best_sel, cur, n*sizeof(int));
                psi_improvements++;
                residual *= 0.85;
                coherence += 0.06;
                if (coherence > 1.0) coherence = 1.0;
                no_improve = 0;
            } else {
                residual += 0.03;
                if (residual > 1.0) residual = 1.0;
                coherence -= 0.025;
                if (coherence < 0.0) coherence = 0.0;
                no_improve++;
            }
            if (no_improve > 30) break;
        }
        psi_sum += psi_val;
        psi_imp += psi_improvements;
        psi_N += psi_sum_N;
        memcpy(sel_noisy, best_sel, n*sizeof(int));
        for (int i = 0; i < 50; i++) { int idx = rand() % n; sel_noisy[idx] = sel_noisy[idx] ? 0 : 1; }
        noisy_s = evaluate(&kp, sel_noisy);
        while (noisy_s.weight > kp.capacity) { int idx = rand() % n; if (sel_noisy[idx]) { sel_noisy[idx] = 0; noisy_s = evaluate(&kp, sel_noisy); } }
        double resil_psi = (noisy_s.value >= psi_val) ? 1.0 : 0.0;
        psi_resil += resil_psi;
        write_csv_row(csv, seed, "PSI", psi_val, psi_improvements, psi_sum_N, resil_psi);

        if (psi_val > grls_val) psi_better++;
        else if (psi_val == grls_val) psi_equal++;
        else psi_worse++;
    }

    fclose(csv);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   PSI LEARNING CURVE vs BASELINE (N=%d)      ║\n", INSTANCES);
    printf("╚══════════════════════════════════════════════╝\n\n");
    printf("Method     | Avg Value | Avg Improvements | Avg sum_N | Resilience\n");
    printf("───────────┼───────────┼─────────────────┼───────────┼───────────\n");
    printf("GR-B       | %9.1f |       %9.1f | %9.1f | %9.3f\n", grb_sum/INSTANCES, grb_imp/INSTANCES, grb_N/INSTANCES, grb_resil/INSTANCES);
    printf("GR-LS      | %9.1f |       %9.1f | %9.1f | %9.3f\n", grls_sum/INSTANCES, grls_imp/INSTANCES, grls_N/INSTANCES, grls_resil/INSTANCES);
    printf("PSI-CORE   | %9.1f |       %9.1f | %9.1f | %9.3f\n", psi_sum/INSTANCES, psi_imp/INSTANCES, psi_N/INSTANCES, psi_resil/INSTANCES);
    printf("───────────┴───────────┴─────────────────┴───────────┴───────────\n");
    printf("PSI vs GR-LS : better=%d  equal=%d  worse=%d\n", psi_better, psi_equal, psi_worse);
    printf("\n✅ Full learning curve saved to logs/psi_learning_curve.csv\n");
    return 0;
}
