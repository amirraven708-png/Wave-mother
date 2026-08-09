#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"

typedef struct {
    int value;
    int weight;
} solution_stats_t;

/* ------------------------------------------------------------
 * Evaluate a selection
 * ------------------------------------------------------------ */
static solution_stats_t evaluate(
    const knapsack_problem_t *kp,
    const int *selected)
{
    solution_stats_t s = {0, 0};
    for (int i = 0; i < kp->count; i++) {
        if (selected[i]) {
            s.value += kp->items[i].value;
            s.weight += kp->items[i].weight;
        }
    }
    return s;
}

static int validate_solution(const knapsack_problem_t *kp, const int *selected) {
    solution_stats_t s = evaluate(kp, selected);
    return s.weight <= kp->capacity;
}

static void repair_solution(const knapsack_problem_t *kp, int *selected) {
    solution_stats_t s = evaluate(kp, selected);
    while (s.weight > kp->capacity) {
        int worst = -1;
        double worst_ratio = 1e30;
        for (int i = 0; i < kp->count; i++) {
            if (!selected[i]) continue;
            double ratio = (double)kp->items[i].value / kp->items[i].weight;
            if (ratio < worst_ratio) { worst_ratio = ratio; worst = i; }
        }
        if (worst < 0) break;
        selected[worst] = 0;
        s = evaluate(kp, selected);
    }
}

static void build_greedy(const knapsack_problem_t *kp, int *selected) {
    int n = kp->count;
    int order[MAX_ITEMS];
    double ratio[MAX_ITEMS];
    for (int i = 0; i < n; i++) {
        order[i] = i;
        ratio[i] = (double)kp->items[i].value / kp->items[i].weight;
    }
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (ratio[order[j]] > ratio[order[i]]) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
    memset(selected, 0, n * sizeof(int));
    int weight = 0;
    for (int i = 0; i < n; i++) {
        int idx = order[i];
        if (weight + kp->items[idx].weight <= kp->capacity) {
            selected[idx] = 1;
            weight += kp->items[idx].weight;
        }
    }
}

static int improve_1x1(const knapsack_problem_t *kp, int *selected) {
    solution_stats_t current = evaluate(kp, selected);
    int improved = 0;
    for (int out = 0; out < kp->count; out++) {
        if (!selected[out]) continue;
        for (int in = 0; in < kp->count; in++) {
            if (selected[in]) continue;
            int new_weight = current.weight - kp->items[out].weight + kp->items[in].weight;
            if (new_weight > kp->capacity) continue;
            int new_value = current.value - kp->items[out].value + kp->items[in].value;
            if (new_value > current.value) {
                selected[out] = 0; selected[in] = 1;
                current.weight = new_weight; current.value = new_value;
                improved = 1;
            }
        }
    }
    return improved;
}

static void psi_perturb(const knapsack_problem_t *kp, int *selected, double N) {
    int attempts = (int)(N * 0.5);
    if (attempts < 1) attempts = 1;
    if (attempts > kp->count) attempts = kp->count;
    for (int k = 0; k < attempts; k++) {
        int idx = rand() % kp->count;
        selected[idx] = selected[idx] ? 0 : 1;
    }
    repair_solution(kp, selected);
}

static int knapsack_psi_solve_seed(const knapsack_problem_t *kp, int *best, 
                                   int *repairs_count, double *final_N, 
                                   double *final_coh) {
    psi_core_engine_t psi;
    psi_core_init(&psi);
    
    int n = kp->count;
    int current[MAX_ITEMS];
    
    build_greedy(kp, current);
    repair_solution(kp, current);
    while (improve_1x1(kp, current)) repair_solution(kp, current);
    
    memcpy(best, current, n * sizeof(int));
    solution_stats_t best_stats = evaluate(kp, best);
    
    double residual = 0.1;
    double coherence = 0.5;
    int infeasible_count = 0;
    
    for (int iter = 0; iter < 50; iter++) {
        psi_core_breathe(&psi, residual, coherence, 0.25);
        double N = psi.dimensional_state;
        
        memcpy(current, best, n * sizeof(int));
        psi_perturb(kp, current, N);
        while (improve_1x1(kp, current));
        
        solution_stats_t candidate = evaluate(kp, current);
        
        if (!validate_solution(kp, current)) {
            repair_solution(kp, current);
            candidate = evaluate(kp, current);
            infeasible_count++;
            if (!validate_solution(kp, current)) continue;
        }
        
        if (candidate.value > best_stats.value) {
            memcpy(best, current, n * sizeof(int));
            best_stats = candidate;
            residual *= 0.8;
            coherence += 0.1;
        } else {
            residual += 0.02;
            coherence -= 0.05;
        }
        
        if (residual < 0.01) residual = 0.01;
        if (residual > 1.0) residual = 1.0;
        if (coherence < 0.0) coherence = 0.0;
        if (coherence > 1.0) coherence = 1.0;
        
        if (residual < 0.02 && coherence > 0.9) break;
    }
    
    *repairs_count = infeasible_count;
    *final_N = psi.dimensional_state;
    *final_coh = psi.coherence;
    
    return best_stats.value;
}

int main(void) {
    #define NUM_SEEDS 100
    
    int dp_values[NUM_SEEDS];
    int psi_values[NUM_SEEDS];
    int repairs[NUM_SEEDS];
    double final_Ns[NUM_SEEDS];
    double final_cohs[NUM_SEEDS];
    int exact_hits = 0;
    int feasible_count = 0;
    
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║     PSI-CORE 100-SEED DIVERSE BENCHMARK             ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    // Show diversity first
    printf("🔍 PROBLEM DIVERSITY CHECK:\n");
    for (int s = 0; s < 5; s++) {
        knapsack_problem_t test_kp;
        knapsack_generate(&test_kp, 200, 50, 200, 1000, s + 1000);
        printf("   Seed %d: item0=(w=%d,v=%d) item1=(w=%d,v=%d)\n",
               s + 1, test_kp.items[0].weight, test_kp.items[0].value,
               test_kp.items[1].weight, test_kp.items[1].value);
    }
    printf("\n");
    
    for (int s = 0; s < NUM_SEEDS; s++) {
        int seed = s + 1000;  // Different base seed
        
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, seed);
        
        int selected[MAX_ITEMS];
        dp_values[s] = knapsack_dp_exact(&kp, selected);
        
        int best[MAX_ITEMS];
        int rep_count;
        double fn, fc;
        
        psi_values[s] = knapsack_psi_solve_seed(&kp, best, &rep_count, &fn, &fc);
        repairs[s] = rep_count;
        final_Ns[s] = fn;
        final_cohs[s] = fc;
        
        if (psi_values[s] == dp_values[s]) exact_hits++;
        if (psi_values[s] > 0) feasible_count++;  // Simplified
        
        if ((s + 1) % 10 == 0) {
            printf("Progress: %d/%d seeds\n", s + 1, NUM_SEEDS);
        }
    }
    
    // Statistics
    double total_gap = 0, min_gap = 1e30, max_gap = -1e30;
    for (int i = 0; i < NUM_SEEDS; i++) {
        double gap = (double)(dp_values[i] - psi_values[i]) / dp_values[i] * 100.0;
        total_gap += gap;
        if (gap < min_gap) min_gap = gap;
        if (gap > max_gap) max_gap = gap;
    }
    
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║     RESULTS                                         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    printf("📊 GAP STATISTICS:\n");
    printf("   Best gap:     %.4f%%\n", min_gap);
    printf("   Mean gap:     %.4f%%\n", total_gap / NUM_SEEDS);
    printf("   Worst gap:    %.4f%%\n", max_gap);
    
    printf("\n🎯 PERFORMANCE:\n");
    printf("   Exact hits:   %d/%d (%.1f%%)\n", exact_hits, NUM_SEEDS, 
           (double)exact_hits / NUM_SEEDS * 100.0);
    printf("   Mean repairs: %.1f\n", 
           (double)total_gap / NUM_SEEDS > 0 ? total_gap / NUM_SEEDS : 0.0);
    
    // Show sample
    printf("\n📋 SAMPLE RESULTS:\n");
    printf("   Seed | DP_Val | PSI_Val | Gap%% | Repairs | N | Coh\n");
    printf("   -----|--------|---------|------|---------|------|-----\n");
    for (int i = 0; i < 5; i++) {
        double gap = (double)(dp_values[i] - psi_values[i]) / dp_values[i] * 100.0;
        printf("   %4d | %6d | %7d | %4.2f%% | %7d | %4.1f | %.3f\n",
               i + 1000, dp_values[i], psi_values[i], gap, repairs[i], 
               final_Ns[i], final_cohs[i]);
    }
    
    // Save CSV
    FILE *fp = fopen("psi_100seed_results.csv", "w");
    if (fp) {
        fprintf(fp, "seed,dp_value,psi_value,gap_percent,repairs,final_N,final_coherence\n");
        for (int i = 0; i < NUM_SEEDS; i++) {
            double gap = (double)(dp_values[i] - psi_values[i]) / dp_values[i] * 100.0;
            fprintf(fp, "%d,%d,%d,%.4f,%d,%.2f,%.4f\n",
                    i + 1000, dp_values[i], psi_values[i], gap, repairs[i],
                    final_Ns[i], final_cohs[i]);
        }
        fclose(fp);
        printf("\n📁 Results saved to psi_100seed_results.csv\n");
    }
    
    return 0;
}
