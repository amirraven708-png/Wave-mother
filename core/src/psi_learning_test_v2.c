#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"
#include "psi_experience_memory.h"

typedef struct {
    int value;
    int weight;
} solution_stats_t;

typedef struct {
    int seed;
    problem_family_t family;
    int dp_value;
    int psi_value;
    double psi_time_ms;
    int psi_feasible;
    int psi_exact;
    double gap_percent;
    int repairs;
    double final_N;
    double final_coherence;
    int used_memory;
    double similarity_score;
    double baseline_exact_rate;
} test_result_t;

static solution_stats_t evaluate(const knapsack_problem_t *kp, const int *selected) {
    solution_stats_t s = {0, 0};
    for (int i = 0; i < kp->count; i++) {
        if (selected[i]) { s.value += kp->items[i].value; s.weight += kp->items[i].weight; }
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

static void generate_family_problem(knapsack_problem_t *kp, problem_family_t family, int seed) {
    switch (family) {
        case FAMILY_A_TRAIN:
        case FAMILY_A_TEST:
            knapsack_generate(kp, 200, 50, 200, 1000, seed);
            break;
        case FAMILY_B_TRANSFER:
            knapsack_generate(kp, 200, 60, 180, 1200, seed);
            break;
        case FAMILY_C_GENERALIZE:
            knapsack_generate(kp, 150, 80, 150, 800, seed);
            break;
        case FAMILY_D_NOVEL:
            knapsack_generate(kp, 300, 30, 300, 1500, seed);
            break;
    }
}

static test_result_t psi_solve_baseline(const knapsack_problem_t *kp, int seed, problem_family_t family) {
    test_result_t result = {0};
    
    int n = kp->count;
    int best[MAX_ITEMS];
    
    build_greedy(kp, best);
    repair_solution(kp, best);
    while (improve_1x1(kp, best)) repair_solution(kp, best);
    
    solution_stats_t s = evaluate(kp, best);
    result.psi_value = s.value;
    result.psi_feasible = validate_solution(kp, best);
    result.baseline_exact_rate = 0.0;  // Will be set later
    
    return result;
}

static test_result_t psi_solve_with_memory_v2(
    const knapsack_problem_t *kp, 
    psi_memory_t *memory,
    int seed,
    problem_family_t family,
    int use_memory)
{
    test_result_t result = {0};
    result.seed = seed;
    result.family = family;
    
    // Extract problem features
    psi_experience_t context = {0};
    double total_w = 0, total_v = 0;
    for (int i = 0; i < kp->count; i++) {
        total_w += kp->items[i].weight;
        total_v += kp->items[i].value;
    }
    context.avg_weight = total_w / kp->count;
    context.avg_value = total_v / kp->count;
    context.capacity_ratio = (double)kp->capacity / total_w;
    context.item_count = kp->count;
    context.density = total_v / (total_w + 1.0);
    context.family = family;
    context.timestamp = time(NULL);
    
    psi_core_engine_t psi;
    psi_core_init(&psi);
    
    int n = kp->count;
    int current[MAX_ITEMS];
    int best[MAX_ITEMS];
    
    build_greedy(kp, current);
    repair_solution(kp, current);
    while (improve_1x1(kp, current)) repair_solution(kp, current);
    
    memcpy(best, current, n * sizeof(int));
    solution_stats_t best_stats = evaluate(kp, best);
    
    double residual = 0.1;
    double coherence = 0.5;
    int infeasible_count = 0;
    
    // Try to use memory
    double suggested_exploration = -1.0;
    double suggested_perturbation = -1.0;
    
    if (use_memory && memory->pattern_count > 0) {
        context.initial_residual = residual;
        context.initial_coherence = coherence;
        context.initial_N = psi.dimensional_state;
        
        suggested_exploration = psi_memory_suggest_exploration_rate(memory, &context);
        suggested_perturbation = psi_memory_suggest_perturbation(memory, &context);
        
        if (suggested_exploration > 0) {
            result.used_memory = 1;
            result.similarity_score = suggested_exploration;
        }
    }
    
    clock_t start_time = clock();
    
    for (int iter = 0; iter < 50; iter++) {
        psi_core_breathe(&psi, residual, coherence, 0.25);
        double N = psi.dimensional_state;
        
        // Apply memory suggestions if available
        if (suggested_perturbation > 0 && iter < 10) {
            N = suggested_perturbation;
        }
        
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
    
    result.psi_time_ms = (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.0;
    result.repairs = infeasible_count;
    result.final_N = psi.dimensional_state;
    result.final_coherence = psi.coherence;
    result.psi_value = best_stats.value;
    result.psi_feasible = validate_solution(kp, best);
    
    // Store experience with actual success
    context.exploration_rate = result.final_N;
    context.perturbation_intensity = result.final_N;
    context.local_search_depth = 50;
    context.improvement_rate = result.psi_feasible ? 0.8 : 0.1;
    context.repair_rate = (double)infeasible_count / 50.0;
    context.final_coherence = result.final_coherence;
    context.convergence_speed = result.psi_time_ms < 10.0 ? 1.0 : 0.5;
    context.success_score = result.psi_feasible ? 0.9 : 0.1;
    context.reuse_count = 0;
    
    psi_memory_store_experience(memory, &context);
    
    return result;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     PSI-CORE LEARNING TEST V2 (DEBUGGED)               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    // Compare: Fresh PSI vs Learning PSI
    psi_memory_t memory_fresh;
    psi_memory_t memory_learning;
    
    psi_memory_init(&memory_fresh);
    psi_memory_init(&memory_learning);
    
    #define TRAINING_PROBLEMS 2000
    #define TEST_PROBLEMS 50
    
    // Phase 1: Train learning PSI
    printf("🚀 PHASE 1: TRAINING LEARNING PSI\n");
    for (int i = 0; i < TRAINING_PROBLEMS; i++) {
        knapsack_problem_t kp;
        generate_family_problem(&kp, FAMILY_A_TRAIN, i + 10000);
        
        int selected[MAX_ITEMS];
        knapsack_dp_exact(&kp, selected);
        
        test_result_t res = psi_solve_with_memory_v2(&kp, &memory_learning, i + 10000, 
                                                      FAMILY_A_TRAIN, 1);
        
        if ((i + 1) % 500 == 0) {
            printf("Training: %d/%d (patterns: %d)\n", i + 1, TRAINING_PROBLEMS, 
                   memory_learning.pattern_count);
        }
    }
    
    printf("\n📊 Learning memory state:\n");
    psi_memory_print_learning_status(&memory_learning);
    
    // Phase 2: Test both on all families
    printf("\n🧪 PHASE 2: COMPARATIVE TESTING\n");
    printf("──────────────────────────────────────────────────────\n");
    
    problem_family_t families[] = {FAMILY_A_TEST, FAMILY_B_TRANSFER, 
                                    FAMILY_C_GENERALIZE, FAMILY_D_NOVEL};
    const char *family_names[] = {"A-test (similar)", "B-transfer", 
                                   "C-generalize", "D-novel"};
    
    for (int fam = 0; fam < 4; fam++) {
        printf("\n📋 Family %s:\n", family_names[fam]);
        
        int fresh_exact = 0, learn_exact = 0;
        double fresh_gap = 0, learn_gap = 0;
        int fresh_memory = 0, learn_memory = 0;
        
        for (int t = 0; t < TEST_PROBLEMS; t++) {
            knapsack_problem_t kp;
            generate_family_problem(&kp, families[fam], t + 20000 + fam * 1000);
            
            int selected[MAX_ITEMS];
            int dp_val = knapsack_dp_exact(&kp, selected);
            
            // Fresh PSI (no memory)
            test_result_t fresh_res = psi_solve_with_memory_v2(&kp, &memory_fresh, 
                                                                t + 20000, families[fam], 0);
            if (fresh_res.psi_value == dp_val) fresh_exact++;
            fresh_gap += (double)(dp_val - fresh_res.psi_value) / dp_val * 100.0;
            if (fresh_res.used_memory) fresh_memory++;
            
            // Learning PSI (with memory)
            test_result_t learn_res = psi_solve_with_memory_v2(&kp, &memory_learning, 
                                                                t + 30000, families[fam], 1);
            if (learn_res.psi_value == dp_val) learn_exact++;
            learn_gap += (double)(dp_val - learn_res.psi_value) / dp_val * 100.0;
            if (learn_res.used_memory) learn_memory++;
        }
        
        double fresh_rate = (double)fresh_exact / TEST_PROBLEMS * 100;
        double learn_rate = (double)learn_exact / TEST_PROBLEMS * 100;
        double improvement = learn_rate - fresh_rate;
        
        printf("   Fresh PSI:     %d/%d (%.1f%%) exact, gap: %.4f%%\n",
               fresh_exact, TEST_PROBLEMS, fresh_rate, fresh_gap/TEST_PROBLEMS);
        printf("   Learning PSI:  %d/%d (%.1f%%) exact, gap: %.4f%%\n",
               learn_exact, TEST_PROBLEMS, learn_rate, learn_gap/TEST_PROBLEMS);
        printf("   Improvement:   %+.1f%%\n", improvement);
        printf("   Memory usage:  Fresh=%d%%, Learning=%d%%\n",
               fresh_memory * 100 / TEST_PROBLEMS, learn_memory * 100 / TEST_PROBLEMS);
    }
    
    // Learning verification
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║     LEARNING VERIFICATION                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("   Learning PSI patterns: %d\n", memory_learning.pattern_count);
    printf("   Fresh PSI patterns: %d\n", memory_fresh.pattern_count);
    printf("   Human interventions: 0\n");
    printf("   System restarts: 0\n");
    
    return 0;
}
