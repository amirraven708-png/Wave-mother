#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"

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

static void log_episode(FILE *fp, int ep, int val, double residual, double coherence, double N) {
    fprintf(fp, "%d,%d,%.4f,%.4f,%.2f\n", ep, val, residual, coherence, N);
}

int main() {
    srand(42); // fixed seed for reproducibility
    knapsack_problem_t kp;
    knapsack_generate(&kp, 200, 50, 200, 1000);
    int n = kp.count;

    // Baseline: greedy
    int greedy_sel[MAX_ITEMS];
    build_greedy(&kp, greedy_sel);
    sol_stats_t greedy_s = evaluate(&kp, greedy_sel);
    printf("Baseline greedy: value=%d weight=%d\n", greedy_s.value, greedy_s.weight);

    // DP exact (just for reference)
    int dp_sel[MAX_ITEMS];
    int dp_val = knapsack_dp_exact(&kp, dp_sel);
    printf("DP exact:        value=%d\n", dp_val);

    // PSI-CORE learner
    psi_core_engine_t psi;
    psi_core_init(&psi);
    double residual = 1.0, coherence = 0.3;
    int best_sel[MAX_ITEMS];
    sol_stats_t best_s = {0,0};
    int no_improve = 0;

    FILE *log = fopen("learning_curve.csv", "w");
    if (!log) { perror("learning_curve.csv"); return 1; }
    fprintf(log, "episode,value,residual,coherence,N\n");

    int episodes = 100;
    for (int ep = 1; ep <= episodes; ep++) {
        // Use current best as starting point (or greedy for first)
        int cur[MAX_ITEMS];
        if (ep == 1) {
            memcpy(cur, greedy_sel, n*sizeof(int));
            best_s = greedy_s;
            memcpy(best_sel, greedy_sel, n*sizeof(int));
        } else {
            memcpy(cur, best_sel, n*sizeof(int));
        }

        // Breathe PSI
        psi_core_breathe(&psi, residual, coherence, 0.5);
        double N = psi.dimensional_state;

        // Perturb
        psi_perturb(&kp, cur, N);
        while (improve_1x1(&kp, cur));

        sol_stats_t cur_s = evaluate(&kp, cur);
        double improvement = cur_s.value - best_s.value;

        if (cur_s.value > best_s.value) {
            best_s = cur_s;
            memcpy(best_sel, cur, n*sizeof(int));
            // Progress!
            if (residual > 0.05) residual *= 0.9;
            coherence += 0.05;
            if (coherence > 1.0) coherence = 1.0;
            no_improve = 0;
        } else {
            // No progress
            residual += 0.02;
            if (residual > 1.0) residual = 1.0;
            coherence -= 0.02;
            if (coherence < 0.0) coherence = 0.0;
            no_improve++;
        }

        log_episode(log, ep, best_s.value, residual, coherence, N);

        // Early stop if stuck
        if (no_improve > 30 && best_s.value >= greedy_s.value) break;
    }

    fclose(log);

    printf("\n--- Learning curve saved to learning_curve.csv ---\n");
    printf("Final PSI best: value=%d weight=%d\n", best_s.value, best_s.weight);
    printf("Greedy:         value=%d\n", greedy_s.value);
    printf("DP exact:       value=%d\n", dp_val);
    printf("Improvement over greedy: %+d (%.2f%%)\n", 
           best_s.value - greedy_s.value,
           (best_s.value - greedy_s.value)*100.0 / greedy_s.value);
    printf("Gap to DP: %d (%.2f%%)\n", dp_val - best_s.value, (dp_val - best_s.value)*100.0/dp_val);

    // Print first and last 5 episodes of learning curve
    printf("\nLearning curve (first 5 / last 5):\n");
    FILE *rlog = fopen("learning_curve.csv", "r");
    if (rlog) {
        char line[256];
        int lineno = 0;
        // skip header
        fgets(line, sizeof(line), rlog);
        while (fgets(line, sizeof(line), rlog)) {
            lineno++;
            if (lineno <= 5 || lineno > episodes-5) printf("%s", line);
        }
        fclose(rlog);
    }

    return 0;
}
