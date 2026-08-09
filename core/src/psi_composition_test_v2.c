#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"
#include "psi_pattern_tree.h"

typedef struct {
    int value;
    int weight;
} solution_stats_t;

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

// PSI Solver that uses Pattern Tree for composition
static int psi_solve_with_composition(
    const knapsack_problem_t *kp,
    pattern_tree_t *tree,
    int *best_solution,
    double *time_ms,
    int *used_composition)
{
    // Extract features
    double features[5];
    double total_w = 0, total_v = 0;
    for (int i = 0; i < kp->count; i++) {
        total_w += kp->items[i].weight;
        total_v += kp->items[i].value;
    }
    features[0] = total_w / kp->count / 50.0;
    features[1] = total_v / kp->count / 200.0;
    features[2] = (double)kp->capacity / total_w;
    features[3] = kp->count / 200.0;
    features[4] = total_v / (total_w + 1.0) / 5.0;
    
    int sequence[MAX_PATH_LENGTH];
    int seq_length;
    double estimated_time;
    
    double target_outcome[3] = {0.9, 0.8, 0.95};
    
    *used_composition = pattern_tree_find_shortest_path(tree, features, target_outcome,
                                                         sequence, &seq_length);
    
    clock_t start_time = clock();
    
    int n = kp->count;
    int best[MAX_ITEMS];
    
    if (*used_composition && seq_length == 1) {
        psi_pattern_t *pat = &tree->confirmed_patterns[sequence[0]];
        
        build_greedy(kp, best);
        repair_solution(kp, best);
        
        for (int i = 0; i < pat->decision_count && i < kp->count; i++) {
            if (pat->decisions[i] == 1) {
                best[i] = 1;
            }
        }
        repair_solution(kp, best);
    } else if (*used_composition && seq_length > 1) {
        build_greedy(kp, best);
        repair_solution(kp, best);
        
        for (int s = 0; s < seq_length; s++) {
            psi_pattern_t *pat = &tree->confirmed_patterns[sequence[s]];
            for (int i = 0; i < pat->decision_count && i < kp->count; i++) {
                if (pat->decisions[i] == 1) best[i] = 1;
            }
            repair_solution(kp, best);
        }
    } else {
        build_greedy(kp, best);
        repair_solution(kp, best);
    }
    
    while (improve_1x1(kp, best)) repair_solution(kp, best);
    
    *time_ms = (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.0;
    
    memcpy(best_solution, best, n * sizeof(int));
    return evaluate(kp, best).value;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     PSI PATTERN COMPOSITION LEARNING TEST              ║\n");
    printf("║     (Tree-based hierarchical learning)                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    pattern_tree_t tree;
    pattern_tree_init(&tree);
    
    #define TRAINING_SIZE 500
    #define TEST_SIZE 50
    
    printf("🚀 PHASE 1: PATTERN DISCOVERY (%d problems)\n", TRAINING_SIZE);
    printf("──────────────────────────────────────────────────────\n");
    
    int patterns_added = 0;
    double quality_curve[5] = {0};
    int checkpoints[] = {100, 200, 300, 400, 500};
    
    for (int i = 0; i < TRAINING_SIZE; i++) {
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, i + 10000);  // Family A
        
        int selected[MAX_ITEMS];
        int dp_val = knapsack_dp_exact(&kp, selected);
        
        int best[MAX_ITEMS];
        double time_ms;
        int used_comp;
        
        int psi_val = psi_solve_with_composition(&kp, &tree, best, &time_ms, &used_comp);
        
        // Extract pattern
        double features[5];
        double total_w = 0, total_v = 0;
        for (int j = 0; j < kp.count; j++) {
            total_w += kp.items[j].weight;
            total_v += kp.items[j].value;
        }
        features[0] = total_w / kp.count / 50.0;
        features[1] = total_v / kp.count / 200.0;
        features[2] = (double)kp.capacity / total_w;
        features[3] = kp.count / 200.0;
        features[4] = total_v / (total_w + 1.0) / 5.0;
        
        int decisions[10];
        int dec_count = kp.count < 10 ? kp.count : 10;
        for (int j = 0; j < dec_count; j++) {
            decisions[j] = best[j];
        }
        
        double outcome[3];
        int exact = (psi_val == dp_val) ? 1 : 0;
        outcome[0] = exact ? 1.0 : 0.8;
        outcome[1] = time_ms < 5.0 ? 1.0 : 0.5;
        outcome[2] = exact ? 1.0 : (double)psi_val / dp_val;
        
        int pat_id = pattern_tree_add_pattern(&tree, features, decisions, dec_count, 
                                               outcome, time_ms, outcome[2]);
        if (pat_id >= 0) patterns_added++;
        
        if (exact) {
            pattern_tree_confirm_patterns_with_same_outcome(&tree, outcome, 0.2);
        }
        
        if (used_comp && exact && time_ms < 10.0) {
            int sequence[2] = {pat_id, pat_id};
            pattern_tree_learn_composition(&tree, sequence, 2, 1.0, time_ms);
        }
        
        // Checkpoints
        for (int c = 0; c < 5; c++) {
            if (i + 1 == checkpoints[c]) {
                int hits = 0;
                for (int t = 0; t < 20; t++) {
                    knapsack_problem_t test_kp;
                    knapsack_generate(&test_kp, 200, 50, 200, 1000, t + 50000);  // Family A test
                    
                    int test_sel[MAX_ITEMS];
                    int test_dp = knapsack_dp_exact(&test_kp, test_sel);
                    
                    int test_best[MAX_ITEMS];
                    double test_time;
                    int test_comp;
                    
                    int test_val = psi_solve_with_composition(&test_kp, &tree, test_best, 
                                                              &test_time, &test_comp);
                    if (test_val == test_dp) hits++;
                }
                quality_curve[c] = (double)hits / 20.0 * 100.0;
                
                printf("Checkpoint %d: %d/20 exact (%.1f%%), patterns: %d, compositions: %d\n",
                       i + 1, hits, quality_curve[c], tree.confirmed_count, tree.composition_count);
            }
        }
    }
    
    printf("\n📊 Pattern Tree Statistics:\n");
    pattern_tree_print_stats(&tree);
    
    printf("\n🧪 PHASE 2: COMPOSITION TESTING ON DIFFERENT FAMILIES\n");
    printf("──────────────────────────────────────────────────────\n");
    
    // Test on 4 different problem families
    struct {
        int count;
        int max_w;
        int max_v;
        int capacity;
        int seed_offset;
        const char *name;
    } families[] = {
        {200, 50, 200, 1000, 20000, "A-test (similar)"},
        {200, 60, 180, 1200, 21000, "B-transfer"},
        {150, 80, 150, 800,  22000, "C-generalize"},
        {300, 30, 300, 1500, 23000, "D-novel"}
    };
    
    for (int fam = 0; fam < 4; fam++) {
        int exact = 0, used_comp_count = 0;
        double total_time = 0;
        
        for (int t = 0; t < TEST_SIZE; t++) {
            knapsack_problem_t kp;
            knapsack_generate(&kp, families[fam].count, families[fam].max_w,
                             families[fam].max_v, families[fam].capacity,
                             families[fam].seed_offset + t);
            
            int selected[MAX_ITEMS];
            int dp_val = knapsack_dp_exact(&kp, selected);
            
            int best[MAX_ITEMS];
            double time_ms;
            int used_comp;
            
            int psi_val = psi_solve_with_composition(&kp, &tree, best, &time_ms, &used_comp);
            
            if (psi_val == dp_val) exact++;
            if (used_comp) used_comp_count++;
            total_time += time_ms;
        }
        
        printf("\n📋 Family %s:\n", families[fam].name);
        printf("   Exact hits:     %d/%d (%.1f%%)\n", exact, TEST_SIZE, (double)exact/TEST_SIZE*100);
        printf("   Compositions:   %d/%d (%.1f%%)\n", used_comp_count, TEST_SIZE, 
               (double)used_comp_count/TEST_SIZE*100);
        printf("   Avg time:       %.3f ms\n", total_time/TEST_SIZE);
    }
    
    printf("\n📈 LEARNING CURVE (Pattern Tree):\n");
    printf("   Problems | Exact Hit Rate\n");
    printf("   ---------|----------------\n");
    for (int c = 0; c < 5; c++) {
        printf("   %8d | %.1f%%\n", checkpoints[c], quality_curve[c]);
    }
    
    printf("\n✅ Key Features:\n");
    printf("   • Hierarchical pattern tree with %d nodes\n", tree.node_count);
    printf("   • %d confirmed patterns\n", tree.confirmed_count);
    printf("   • %d learned compositions\n", tree.composition_count);
    printf("   • Automatic pattern confirmation\n");
    printf("   • Shortest-path composition finding\n");
    
    return 0;
}
