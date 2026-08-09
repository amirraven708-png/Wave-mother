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

typedef struct {
    int seed;
    int dp_value;
    
    // Method A: Greedy only
    int greedy_value;
    double greedy_time_ms;
    int greedy_feasible;
    
    // Method B: Greedy + Local Search
    int ls_value;
    double ls_time_ms;
    int ls_feasible;
    int ls_iterations;
    
    // Method C: Greedy + Local Search + PSI
    int psi_value;
    double psi_time_ms;
    int psi_feasible;
    int psi_repairs;
    double psi_final_N;
    double psi_final_coherence;
    int psi_iterations;
    
    // Problem characteristics
    int item0_weight;
    int item0_value;
    int total_items;
    int capacity;
} compare_result_t;

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

/* ------------------------------------------------------------
 * Method A: Greedy Only
 * ------------------------------------------------------------ */
static int method_greedy_only(const knapsack_problem_t *kp, int *selected, double *time_ms) {
    clock_t start = clock();
    
    build_greedy(kp, selected);
    
    *time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    solution_stats_t s = evaluate(kp, selected);
    return s.value;
}

/* ------------------------------------------------------------
 * Method B: Greedy + Local Search
 * ------------------------------------------------------------ */
static int method_greedy_ls(const knapsack_problem_t *kp, int *selected, 
                            double *time_ms, int *iterations) {
    clock_t start = clock();
    
    build_greedy(kp, selected);
    repair_solution(kp, selected);
    
    int iter = 0;
    while (improve_1x1(kp, selected) && iter < 100) {
        repair_solution(kp, selected);
        iter++;
    }
    
    *time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    *iterations = iter;
    
    solution_stats_t s = evaluate(kp, selected);
    return s.value;
}

/* ------------------------------------------------------------
 * Method C: Greedy + Local Search + PSI
 * ------------------------------------------------------------ */
static int method_psi(const knapsack_problem_t *kp, int *selected,
                      double *time_ms, int *repairs, double *final_N,
                      double *final_coh, int *iterations) {
    clock_t start = clock();
    
    psi_core_engine_t psi;
    psi_core_init(&psi);
    
    int n = kp->count;
    int current[MAX_ITEMS];
    
    // Start with greedy + local search
    build_greedy(kp, current);
    repair_solution(kp, current);
    while (improve_1x1(kp, current)) repair_solution(kp, current);
    
    memcpy(selected, current, n * sizeof(int));
    solution_stats_t best_stats = evaluate(kp, selected);
    
    double residual = 0.1;
    double coherence = 0.5;
    int infeasible_count = 0;
    int iter_count = 0;
    
    for (int iter = 0; iter < 50; iter++) {
        psi_core_breathe(&psi, residual, coherence, 0.25);
        double N = psi.dimensional_state;
        
        memcpy(current, selected, n * sizeof(int));
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
            memcpy(selected, current, n * sizeof(int));
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
        
        iter_count++;
        if (residual < 0.02 && coherence > 0.9) break;
    }
    
    *time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    *repairs = infeasible_count;
    *final_N = psi.dimensional_state;
    *final_coh = psi.coherence;
    *iterations = iter_count;
    
    return best_stats.value;
}

/* ============================================================
 * MAIN - COMPARATIVE ANALYSIS
 * ============================================================ */
int main(void) {
    #define NUM_SEEDS 100
    #define SEED_BASE 2000
    
    compare_result_t results[NUM_SEEDS];
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     METHOD COMPARISON: Greedy vs LS vs PSI              ║\n");
    printf("║     100 diverse knapsack instances                      ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    // Statistics accumulators
    int greedy_exact = 0, ls_exact = 0, psi_exact = 0;
    int greedy_feasible = 0, ls_feasible = 0, psi_feasible = 0;
    double total_greedy_gap = 0, total_ls_gap = 0, total_psi_gap = 0;
    double total_greedy_time = 0, total_ls_time = 0, total_psi_time = 0;
    double total_dp_time = 0;
    
    double best_psi_gap = 1e30, worst_psi_gap = -1e30;
    double best_ls_gap = 1e30, worst_ls_gap = -1e30;
    
    for (int s = 0; s < NUM_SEEDS; s++) {
        int seed = SEED_BASE + s;
        
        // Generate problem
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, seed);
        
        compare_result_t *r = &results[s];
        r->seed = seed;
        r->item0_weight = kp.items[0].weight;
        r->item0_value = kp.items[0].value;
        r->total_items = kp.count;
        r->capacity = kp.capacity;
        
        // DP baseline
        int dp_selected[MAX_ITEMS];
        clock_t dp_start = clock();
        r->dp_value = knapsack_dp_exact(&kp, dp_selected);
        double dp_time = (double)(clock() - dp_start) / CLOCKS_PER_SEC * 1000.0;
        total_dp_time += dp_time;
        
        // Method A: Greedy Only
        int greedy_selected[MAX_ITEMS];
        r->greedy_value = method_greedy_only(&kp, greedy_selected, &r->greedy_time_ms);
        r->greedy_feasible = validate_solution(&kp, greedy_selected);
        
        // Method B: Greedy + Local Search
        int ls_selected[MAX_ITEMS];
        r->ls_value = method_greedy_ls(&kp, ls_selected, &r->ls_time_ms, &r->ls_iterations);
        r->ls_feasible = validate_solution(&kp, ls_selected);
        
        // Method C: PSI
        int psi_selected[MAX_ITEMS];
        r->psi_value = method_psi(&kp, psi_selected, &r->psi_time_ms, 
                                  &r->psi_repairs, &r->psi_final_N, 
                                  &r->psi_final_coherence, &r->psi_iterations);
        r->psi_feasible = validate_solution(&kp, psi_selected);
        
        // Calculate gaps
        double greedy_gap = (double)(r->dp_value - r->greedy_value) / r->dp_value * 100.0;
        double ls_gap = (double)(r->dp_value - r->ls_value) / r->dp_value * 100.0;
        double psi_gap = (double)(r->dp_value - r->psi_value) / r->dp_value * 100.0;
        
        // Accumulate
        if (r->greedy_value == r->dp_value) greedy_exact++;
        if (r->ls_value == r->dp_value) ls_exact++;
        if (r->psi_value == r->dp_value) psi_exact++;
        
        if (r->greedy_feasible) greedy_feasible++;
        if (r->ls_feasible) ls_feasible++;
        if (r->psi_feasible) psi_feasible++;
        
        total_greedy_gap += greedy_gap;
        total_ls_gap += ls_gap;
        total_psi_gap += psi_gap;
        
        total_greedy_time += r->greedy_time_ms;
        total_ls_time += r->ls_time_ms;
        total_psi_time += r->psi_time_ms;
        
        if (psi_gap < best_psi_gap) best_psi_gap = psi_gap;
        if (psi_gap > worst_psi_gap) worst_psi_gap = psi_gap;
        if (ls_gap < best_ls_gap) best_ls_gap = ls_gap;
        if (ls_gap > worst_ls_gap) worst_ls_gap = ls_gap;
        
        if ((s + 1) % 20 == 0) {
            printf("Progress: %d/%d\n", s + 1, NUM_SEEDS);
        }
    }
    
    // Print comprehensive results
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║     COMPARATIVE RESULTS (100 instances)                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("📊 EXACT HIT RATE (value == DP optimum):\n");
    printf("   Greedy only:        %3d/%d (%5.1f%%)\n", 
           greedy_exact, NUM_SEEDS, (double)greedy_exact/NUM_SEEDS*100);
    printf("   Greedy + LS:        %3d/%d (%5.1f%%)\n", 
           ls_exact, NUM_SEEDS, (double)ls_exact/NUM_SEEDS*100);
    printf("   Greedy + LS + PSI:  %3d/%d (%5.1f%%)\n\n", 
           psi_exact, NUM_SEEDS, (double)psi_exact/NUM_SEEDS*100);
    
    printf("✅ FEASIBILITY RATE:\n");
    printf("   Greedy only:        %3d/%d (%5.1f%%)\n", 
           greedy_feasible, NUM_SEEDS, (double)greedy_feasible/NUM_SEEDS*100);
    printf("   Greedy + LS:        %3d/%d (%5.1f%%)\n", 
           ls_feasible, NUM_SEEDS, (double)ls_feasible/NUM_SEEDS*100);
    printf("   Greedy + LS + PSI:  %3d/%d (%5.1f%%)\n\n", 
           psi_feasible, NUM_SEEDS, (double)psi_feasible/NUM_SEEDS*100);
    
    printf("📈 MEAN GAP FROM OPTIMUM:\n");
    printf("   Greedy only:        %.4f%%\n", total_greedy_gap/NUM_SEEDS);
    printf("   Greedy + LS:        %.4f%% (best: %.4f%%, worst: %.4f%%)\n", 
           total_ls_gap/NUM_SEEDS, best_ls_gap, worst_ls_gap);
    printf("   Greedy + LS + PSI:  %.4f%% (best: %.4f%%, worst: %.4f%%)\n\n", 
           total_psi_gap/NUM_SEEDS, best_psi_gap, worst_psi_gap);
    
    printf("⏱️  MEAN RUNTIME:\n");
    printf("   DP Exact:           %.3f ms\n", total_dp_time/NUM_SEEDS);
    printf("   Greedy only:        %.3f ms (%.1fx vs DP)\n", 
           total_greedy_time/NUM_SEEDS, (total_greedy_time/NUM_SEEDS)/(total_dp_time/NUM_SEEDS));
    printf("   Greedy + LS:        %.3f ms (%.1fx vs DP)\n", 
           total_ls_time/NUM_SEEDS, (total_ls_time/NUM_SEEDS)/(total_dp_time/NUM_SEEDS));
    printf("   Greedy + LS + PSI:  %.3f ms (%.1fx vs DP)\n\n", 
           total_psi_time/NUM_SEEDS, (total_psi_time/NUM_SEEDS)/(total_dp_time/NUM_SEEDS));
    
    printf("🔬 PSI-SPECIFIC METRICS (mean):\n");
    double mean_repairs = 0, mean_N = 0, mean_coh = 0, mean_iter = 0;
    for (int i = 0; i < NUM_SEEDS; i++) {
        mean_repairs += results[i].psi_repairs;
        mean_N += results[i].psi_final_N;
        mean_coh += results[i].psi_final_coherence;
        mean_iter += results[i].psi_iterations;
    }
    printf("   Repairs:            %.1f\n", mean_repairs/NUM_SEEDS);
    printf("   Final N:            %.2f\n", mean_N/NUM_SEEDS);
    printf("   Final coherence:    %.4f\n", mean_coh/NUM_SEEDS);
    printf("   Iterations:         %.1f\n\n", mean_iter/NUM_SEEDS);
    
    // Contribution analysis
    double greedy_to_ls = (double)(ls_exact - greedy_exact) / NUM_SEEDS * 100;
    double ls_to_psi = (double)(psi_exact - ls_exact) / NUM_SEEDS * 100;
    
    printf("💡 CONTRIBUTION ANALYSIS:\n");
    printf("   Local Search adds:  +%.1f%% exact hits over Greedy\n", greedy_to_ls);
    printf("   PSI adds:           +%.1f%% exact hits over LS\n", ls_to_psi);
    printf("   Total improvement:  +%.1f%% over Greedy baseline\n\n", 
           (double)(psi_exact - greedy_exact)/NUM_SEEDS*100);
    
    // Save detailed CSV
    FILE *fp = fopen("knapsack_method_comparison.csv", "w");
    if (fp) {
        fprintf(fp, "seed,dp_value,greedy_value,greedy_time,ls_value,ls_time,ls_iters,"
                "psi_value,psi_time,psi_iters,psi_repairs,psi_N,psi_coh,"
                "item0_w,item0_v,greedy_feasible,ls_feasible,psi_feasible\n");
        
        for (int i = 0; i < NUM_SEEDS; i++) {
            compare_result_t *r = &results[i];
            fprintf(fp, "%d,%d,%d,%.3f,%d,%.3f,%d,%d,%.3f,%d,%d,%.2f,%.4f,%d,%d,%d,%d,%d\n",
                    r->seed, r->dp_value, 
                    r->greedy_value, r->greedy_time_ms,
                    r->ls_value, r->ls_time_ms, r->ls_iterations,
                    r->psi_value, r->psi_time_ms, r->psi_iterations,
                    r->psi_repairs, r->psi_final_N, r->psi_final_coherence,
                    r->item0_weight, r->item0_value,
                    r->greedy_feasible, r->ls_feasible, r->psi_feasible);
        }
        fclose(fp);
        printf("📁 Detailed comparison saved to knapsack_method_comparison.csv\n");
    }
    
    return 0;
}
