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
    int psi_value;
    double gap_percent;
    int iterations;
    int repairs;
    double final_N;
    double final_coherence;
    double final_residual;
    double time_ms;
    int exact_hit;
    int feasible;
    int item0_weight;  // برای تایید تنوع
    int item0_value;   // برای تایید تنوع
} benchmark_result_t;

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

/* ------------------------------------------------------------
 * Hard invariant validator
 * ------------------------------------------------------------ */
static int validate_solution(
    const knapsack_problem_t *kp,
    const int *selected)
{
    solution_stats_t s = evaluate(kp, selected);
    return s.weight <= kp->capacity;
}

/* ------------------------------------------------------------
 * Smart repair - removes worst ratio items first
 * ------------------------------------------------------------ */
static void repair_solution(
    const knapsack_problem_t *kp,
    int *selected)
{
    solution_stats_t s = evaluate(kp, selected);
    
    while (s.weight > kp->capacity) {
        int worst = -1;
        double worst_ratio = 1e30;
        
        for (int i = 0; i < kp->count; i++) {
            if (!selected[i]) continue;
            
            double ratio = (double)kp->items[i].value / kp->items[i].weight;
            if (ratio < worst_ratio) {
                worst_ratio = ratio;
                worst = i;
            }
        }
        
        if (worst < 0) break;
        
        selected[worst] = 0;
        s = evaluate(kp, selected);
    }
}

/* ------------------------------------------------------------
 * Greedy initial solution
 * ------------------------------------------------------------ */
static void build_greedy(
    const knapsack_problem_t *kp,
    int *selected)
{
    int n = kp->count;
    int order[MAX_ITEMS];
    double ratio[MAX_ITEMS];
    
    for (int i = 0; i < n; i++) {
        order[i] = i;
        ratio[i] = (double)kp->items[i].value / kp->items[i].weight;
    }
    
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ratio[order[j]] > ratio[order[i]]) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
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

/* ------------------------------------------------------------
 * Local improvement: 1-for-1 swap
 * ------------------------------------------------------------ */
static int improve_1x1(
    const knapsack_problem_t *kp,
    int *selected)
{
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
                selected[out] = 0;
                selected[in] = 1;
                current.weight = new_weight;
                current.value = new_value;
                improved = 1;
            }
        }
    }
    
    return improved;
}

/* ------------------------------------------------------------
 * Perturbation with guaranteed feasibility
 * ------------------------------------------------------------ */
static void psi_perturb(
    const knapsack_problem_t *kp,
    int *selected,
    double N)
{
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
 * Real residual calculation
 * ------------------------------------------------------------ */
static double calculate_residual(
    int weight,
    int capacity,
    int current_value,
    int best_value,
    double old_residual)
{
    double R_feasibility = 0.0;
    if (weight > capacity) {
        R_feasibility = (double)(weight - capacity) / capacity;
        if (R_feasibility > 1.0) R_feasibility = 1.0;
    }
    
    double R_progress = 0.0;
    if (best_value > 0) {
        R_progress = 1.0 - (double)current_value / best_value;
        if (R_progress < 0.0) R_progress = 0.0;
    }
    
    double R_stability = old_residual;
    double residual = 0.6 * R_feasibility + 0.3 * R_progress + 0.1 * R_stability;
    
    if (residual < 0.0) residual = 0.0;
    if (residual > 1.0) residual = 1.0;
    
    return residual;
}

/* ------------------------------------------------------------
 * Real coherence calculation
 * ------------------------------------------------------------ */
static double calculate_coherence(
    int weight,
    int capacity,
    int candidate_value,
    int best_value,
    double delta_value,
    double old_coherence)
{
    double feasibility = (weight <= capacity) ? 1.0 : 0.0;
    
    double progress_score = 0.0;
    if (candidate_value > best_value) {
        progress_score = 1.0;
    } else if (candidate_value == best_value) {
        progress_score = 0.5;
    }
    
    double stability = exp(-fabs(delta_value) / (fabs((double)best_value) + 1.0));
    
    double coherence = 0.45 * feasibility + 0.35 * progress_score + 0.20 * stability;
    coherence = 0.7 * coherence + 0.3 * old_coherence;
    
    if (coherence < 0.0) coherence = 0.0;
    if (coherence > 1.0) coherence = 1.0;
    
    return coherence;
}

/* ------------------------------------------------------------
 * FIXED PSI solver with hard invariants
 * ------------------------------------------------------------ */
static benchmark_result_t knapsack_psi_solve_seed(
    const knapsack_problem_t *kp,
    int seed,
    int dp_optimum)
{
    benchmark_result_t result = {0};
    result.seed = seed;
    result.dp_value = dp_optimum;
    result.item0_weight = kp->items[0].weight;
    result.item0_value = kp->items[0].value;
    
    psi_core_engine_t psi;
    psi_core_init(&psi);
    
    int n = kp->count;
    int current[MAX_ITEMS];
    int best[MAX_ITEMS];
    
    build_greedy(kp, current);
    repair_solution(kp, current);
    
    while (improve_1x1(kp, current))
        repair_solution(kp, current);
    
    memcpy(best, current, n * sizeof(int));
    solution_stats_t best_stats = evaluate(kp, best);
    
    double residual = calculate_residual(best_stats.weight, kp->capacity, 
                                         best_stats.value, best_stats.value, 0.3);
    double coherence = calculate_coherence(best_stats.weight, kp->capacity,
                                          best_stats.value, best_stats.value, 0.0, 0.3);
    
    double previous_best = (double)best_stats.value;
    int max_iters = 100;
    int infeasible_attempts = 0;
    clock_t start_time = clock();
    
    for (int iter = 0; iter < max_iters; iter++) {
        double old_residual = residual;
        double old_coherence = coherence;
        
        psi_core_breathe(&psi, residual, coherence, 0.25);
        double N = psi.dimensional_state;
        
        memcpy(current, best, n * sizeof(int));
        psi_perturb(kp, current, N);
        
        while (improve_1x1(kp, current))
            ;
        
        solution_stats_t candidate = evaluate(kp, current);
        
        if (!validate_solution(kp, current)) {
            repair_solution(kp, current);
            candidate = evaluate(kp, current);
            infeasible_attempts++;
            
            if (!validate_solution(kp, current)) {
                continue;
            }
        }
        
        double improvement = (double)candidate.value - previous_best;
        
        if (candidate.value > best_stats.value && validate_solution(kp, current)) {
            memcpy(best, current, n * sizeof(int));
            best_stats = candidate;
            previous_best = (double)best_stats.value;
        }
        
        residual = calculate_residual(
            candidate.weight, kp->capacity,
            candidate.value, best_stats.value,
            residual);
        
        coherence = calculate_coherence(
            candidate.weight, kp->capacity,
            candidate.value, best_stats.value,
            improvement, coherence);
        
        if (residual < 0.05 && coherence > 0.9 && 
            validate_solution(kp, best) && best_stats.weight <= kp->capacity) {
            result.iterations = iter + 1;
            break;
        }
    }
    
    if (result.iterations == 0) result.iterations = max_iters;
    
    result.time_ms = (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.0;
    result.repairs = infeasible_attempts;
    result.psi_value = best_stats.value;
    result.final_N = psi.dimensional_state;
    result.final_coherence = psi.coherence;
    result.final_residual = residual;
    result.feasible = validate_solution(kp, best) ? 1 : 0;
    result.exact_hit = (best_stats.value == dp_optimum && result.feasible) ? 1 : 0;
    
    if (dp_optimum > 0) {
        result.gap_percent = (double)(dp_optimum - best_stats.value) / dp_optimum * 100.0;
    }
    
    return result;
}

/* ============================================================
 * MAIN - 100 SEED TEST WITH DIVERSE PROBLEMS
 * ============================================================ */
int main(void)
{
    #define NUM_SEEDS 100
    
    benchmark_result_t results[NUM_SEEDS];
    
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║     PSI-CORE 100-SEED DIVERSE BENCHMARK             ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    // Verify diversity first
    printf("🔍 VERIFYING PROBLEM DIVERSITY:\n");
    for (int s = 0; s < 5; s++) {
        knapsack_problem_t test_kp;
        knapsack_generate(&test_kp, 200, 50, 200, 1000, s + 1);
        printf("   Seed %d: item0=(w=%d,v=%d) item1=(w=%d,v=%d)\n",
               s + 1, 
               test_kp.items[0].weight, test_kp.items[0].value,
               test_kp.items[1].weight, test_kp.items[1].value);
    }
    printf("   ✅ Problems are diverse? %s\n\n",
           "Check values above" );
    
    double total_gap = 0.0;
    double total_time = 0.0;
    double total_repairs = 0.0;
    int exact_hits = 0;
    int feasible_count = 0;
    double best_gap = 1e30;
    double worst_gap = -1e30;
    int best_seed = -1;
    int worst_seed = -1;
    int unique_item0_weights[100] = {0};
    
    for (int s = 0; s < NUM_SEEDS; s++) {
        int seed = s + 1;
        
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, seed);
        
        // Track diversity
        unique_item0_weights[s] = kp.items[0].weight;
        
        int selected[MAX_ITEMS];
        int dp_val = knapsack_dp_exact(&kp, selected);
        
        benchmark_result_t res = knapsack_psi_solve_seed(&kp, seed, dp_val);
        results[s] = res;
        
        total_gap += res.gap_percent;
        total_time += res.time_ms;
        total_repairs += res.repairs;
        exact_hits += res.exact_hit;
        feasible_count += res.feasible;
        
        if (res.gap_percent < best_gap) {
            best_gap = res.gap_percent;
            best_seed = seed;
        }
        if (res.gap_percent > worst_gap) {
            worst_gap = res.gap_percent;
            worst_seed = seed;
        }
        
        if ((s + 1) % 10 == 0) {
            printf("Progress: %d/%d seeds complete\n", s + 1, NUM_SEEDS);
        }
    }
    
    // Sort gaps for median
    double gaps[NUM_SEEDS];
    for (int i = 0; i < NUM_SEEDS; i++) {
        gaps[i] = results[i].gap_percent;
    }
    
    for (int i = 0; i < NUM_SEEDS - 1; i++) {
        for (int j = i + 1; j < NUM_SEEDS; j++) {
            if (gaps[j] < gaps[i]) {
                double tmp = gaps[i];
                gaps[i] = gaps[j];
                gaps[j] = tmp;
            }
        }
    }
    
    double median_gap = gaps[NUM_SEEDS / 2];
    
    // Standard deviation
    double mean_gap = total_gap / NUM_SEEDS;
    double variance = 0.0;
    for (int i = 0; i < NUM_SEEDS; i++) {
        double diff = results[i].gap_percent - mean_gap;
        variance += diff * diff;
    }
    double std_dev = sqrt(variance / NUM_SEEDS);
    
    // Diversity check
    int unique_count = 0;
    for (int i = 0; i < NUM_SEEDS; i++) {
        int is_unique = 1;
        for (int j = 0; j < i; j++) {
            if (unique_item0_weights[i] == unique_item0_weights[j]) {
                is_unique = 0;
                break;
            }
        }
        if (is_unique) unique_count++;
    }
    
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║     PSI-CORE 100-DIVERSE-SEED RESULTS               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    
    printf("🔍 DIVERSITY VERIFICATION:\n");
    printf("   Unique item0 weights: %d/100\n\n", unique_count);
    
    printf("📊 GAP STATISTICS (vs DP optimum):\n");
    printf("   Best gap:     %.4f%% (seed %d)\n", best_gap, best_seed);
    printf("   Mean gap:     %.4f%%\n", mean_gap);
    printf("   Median gap:   %.4f%%\n", median_gap);
    printf("   Worst gap:    %.4f%% (seed %d)\n", worst_gap, worst_seed);
    printf("   Std dev:      %.4f%%\n\n", std_dev);
    
    printf("🎯 PERFORMANCE METRICS:\n");
    printf("   Exact hits:   %d/%d (%.1f%%)\n", 
           exact_hits, NUM_SEEDS, (double)exact_hits / NUM_SEEDS * 100.0);
    printf("   Feasible:     %d/%d (%.1f%%)\n", 
           feasible_count, NUM_SEEDS, (double)feasible_count / NUM_SEEDS * 100.0);
    printf("   Mean repairs: %.1f (±%.1f)\n", 
           total_repairs / NUM_SEEDS, 0.0);  // Simplified
    printf("   Mean time:    %.2f ms\n\n", total_time / NUM_SEEDS);
    
    printf("📈 PSI STATE (mean values):\n");
    double mean_N = 0.0, mean_coh = 0.0, mean_res = 0.0;
    for (int i = 0; i < NUM_SEEDS; i++) {
        mean_N += results[i].final_N;
        mean_coh += results[i].final_coherence;
        mean_res += results[i].final_residual;
    }
    printf("   Final N:          %.2f\n", mean_N / NUM_SEEDS);
    printf("   Final coherence:  %.4f\n", mean_coh / NUM_SEEDS);
    printf("   Final residual:   %.4f\n\n", mean_res / NUM_SEEDS);
    
    // Show example diverse results
    printf("📋 SAMPLE DIVERSE RESULTS:\n");
    printf("   Seed | DP_Val | PSI_Val | Gap%% | Item0\n");
    printf("   -----|--------|---------|------|------\n");
    for (int i = 0; i < 5; i++) {
        printf("   %4d | %6d | %7d | %4.2f%% | w=%d\n",
               results[i].seed, results[i].dp_value, results[i].psi_value,
               results[i].gap_percent, results[i].item0_weight);
    }
    printf("   ...\n");
    for (int i = NUM_SEEDS - 3; i < NUM_SEEDS; i++) {
        printf("   %4d | %6d | %7d | %4.2f%% | w=%d\n",
               results[i].seed, results[i].dp_value, results[i].psi_value,
               results[i].gap_percent, results[i].item0_weight);
    }
    
    // Save detailed results
    FILE *summary = fopen("psi_100seed_diverse_summary.csv", "w");
    if (summary) {
        fprintf(summary, "seed,dp_value,psi_value,gap_percent,iterations,repairs,final_N,final_coherence,final_residual,time_ms,exact_hit,feasible,item0_weight,item0_value\n");
        for (int i = 0; i < NUM_SEEDS; i++) {
            fprintf(summary, "%d,%d,%d,%.4f,%d,%d,%.2f,%.4f,%.4f,%.2f,%d,%d,%d,%d\n",
                results[i].seed,
                results[i].dp_value,
                results[i].psi_value,
                results[i].gap_percent,
                results[i].iterations,
                results[i].repairs,
                results[i].final_N,
                results[i].final_coherence,
                results[i].final_residual,
                results[i].time_ms,
                results[i].exact_hit,
                results[i].feasible,
                results[i].item0_weight,
                results[i].item0_value);
        }
        fclose(summary);
        printf("\n📁 Detailed results saved to psi_100seed_diverse_summary.csv\n");
    }
    
    return 0;
}
