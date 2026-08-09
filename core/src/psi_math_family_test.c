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

// Mathematical family classification
typedef enum {
    MATH_FAMILY_LINEAR = 0,
    MATH_FAMILY_QUADRATIC = 1,
    MATH_FAMILY_EXPONENTIAL = 2,
    MATH_FAMILY_LOGARITHMIC = 3,
    MATH_FAMILY_UNKNOWN = 4
} math_family_t;

typedef struct {
    math_family_t family_type;
    double degree;  // Polynomial degree or function characteristic
    double coefficients[3];  // For function approximation
    int member_count;
    double avg_success_rate;
    double avg_time_ms;
} math_family_group_t;

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

// Classify problem into mathematical family
static math_family_t classify_math_family(const knapsack_problem_t *kp) {
    // Analyze value/weight distribution
    double ratios[MAX_ITEMS];
    for (int i = 0; i < kp->count; i++) {
        ratios[i] = (double)kp->items[i].value / kp->items[i].weight;
    }
    
    // Calculate distribution characteristics
    double mean = 0, variance = 0, skewness = 0;
    for (int i = 0; i < kp->count; i++) mean += ratios[i];
    mean /= kp->count;
    
    for (int i = 0; i < kp->count; i++) {
        double diff = ratios[i] - mean;
        variance += diff * diff;
        skewness += diff * diff * diff;
    }
    variance /= kp->count;
    skewness /= kp->count;
    
    // Classify based on distribution shape
    if (variance < 0.1) return MATH_FAMILY_LINEAR;
    if (fabs(skewness) < 0.5) return MATH_FAMILY_QUADRATIC;
    if (skewness > 1.0) return MATH_FAMILY_EXPONENTIAL;
    if (skewness < -1.0) return MATH_FAMILY_LOGARITHMIC;
    return MATH_FAMILY_UNKNOWN;
}

// Create math family group
static math_family_group_t create_math_family(math_family_t type, const double *features) {
    math_family_group_t group;
    group.family_type = type;
    group.degree = features[4] * 10.0;  // Use density as degree indicator
    group.coefficients[0] = features[0];
    group.coefficients[1] = features[1];
    group.coefficients[2] = features[2];
    group.member_count = 1;
    group.avg_success_rate = 0.0;
    group.avg_time_ms = 0.0;
    return group;
}

// PSI Solver with math family awareness
static int psi_solve_math_family(
    const knapsack_problem_t *kp,
    pattern_tree_t *tree,
    math_family_group_t *family_groups,
    int *family_group_count,
    int *best_solution,
    double *time_ms,
    math_family_t *detected_family)
{
    // Classify problem
    *detected_family = classify_math_family(kp);
    
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
    
    // Find or create math family group
    int group_idx = -1;
    for (int i = 0; i < *family_group_count; i++) {
        if (family_groups[i].family_type == *detected_family) {
            // Check if similar enough to join
            double diff = fabs(family_groups[i].degree - features[4] * 10.0);
            if (diff < 2.0) {
                group_idx = i;
                break;
            }
        }
    }
    
    if (group_idx < 0 && *family_group_count < 20) {
        group_idx = (*family_group_count)++;
        family_groups[group_idx] = create_math_family(*detected_family, features);
    }
    
    // Use pattern tree with family context
    int sequence[MAX_PATH_LENGTH];
    int seq_length;
    double estimated_time;
    
    double target_outcome[3] = {0.9, 0.8, 0.95};
    int used_comp = pattern_tree_find_shortest_path(tree, features, target_outcome,
                                                     sequence, &seq_length);
    
    clock_t start_time = clock();
    
    int n = kp->count;
    int best[MAX_ITEMS];
    
    // Adaptive strategy based on math family
    if (group_idx >= 0 && family_groups[group_idx].member_count > 10) {
        // Use family-specific strategy
        double confidence = family_groups[group_idx].avg_success_rate;
        
        if (confidence > 0.7 && seq_length > 0) {
            // Apply learned pattern
            psi_pattern_t *pat = &tree->confirmed_patterns[sequence[0]];
            build_greedy(kp, best);
            for (int i = 0; i < pat->decision_count && i < kp->count; i++) {
                if (pat->decisions[i] == 1) best[i] = 1;
            }
            repair_solution(kp, best);
        } else {
            // Explore more
            build_greedy(kp, best);
            repair_solution(kp, best);
        }
    } else {
        // New family - explore
        build_greedy(kp, best);
        repair_solution(kp, best);
    }
    
    while (improve_1x1(kp, best)) repair_solution(kp, best);
    
    *time_ms = (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.0;
    
    // Update family group statistics
    if (group_idx >= 0) {
        int exact = (evaluate(kp, best).value == knapsack_dp_exact(kp, (int[MAX_ITEMS]){0})) ? 1 : 0;
        double alpha = 0.1;
        family_groups[group_idx].avg_success_rate = 
            (1 - alpha) * family_groups[group_idx].avg_success_rate + alpha * exact;
        family_groups[group_idx].avg_time_ms = 
            (1 - alpha) * family_groups[group_idx].avg_time_ms + alpha * (*time_ms);
        family_groups[group_idx].member_count++;
    }
    
    // Add pattern to tree with family info
    int decisions[10];
    int dec_count = kp->count < 10 ? kp->count : 10;
    for (int j = 0; j < dec_count; j++) decisions[j] = best[j];
    
    double outcome[3];
    int exact_hit = 0;
    {
        int dp_sel[MAX_ITEMS];
        int dp_val = knapsack_dp_exact(kp, dp_sel);
        exact_hit = (evaluate(kp, best).value == dp_val) ? 1 : 0;
        outcome[0] = exact_hit ? 1.0 : 0.8;
        outcome[1] = *time_ms < 5.0 ? 1.0 : 0.5;
        outcome[2] = exact_hit ? 1.0 : (double)evaluate(kp, best).value / dp_val;
    }
    
    pattern_tree_add_pattern(tree, features, decisions, dec_count, outcome, *time_ms, outcome[2]);
    
    if (exact_hit) {
        pattern_tree_confirm_patterns_with_same_outcome(tree, outcome, 0.2);
    }
    
    memcpy(best_solution, best, n * sizeof(int));
    return evaluate(kp, best).value;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     PSI MATH FAMILY LEARNING TEST                      ║\n");
    printf("║     (Auto-classify + family-specific strategies)       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    pattern_tree_t tree;
    pattern_tree_init(&tree);
    
    math_family_group_t family_groups[20];
    int family_count = 0;
    
    #define TRAINING_SIZE 1000
    #define TEST_SIZE 50
    
    printf("🚀 PHASE 1: MATH FAMILY DISCOVERY (%d problems)\n", TRAINING_SIZE);
    printf("──────────────────────────────────────────────────────\n");
    
    double learning_curve[10] = {0};
    int checkpoints[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    
    for (int i = 0; i < TRAINING_SIZE; i++) {
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, i + 10000);
        
        int best[MAX_ITEMS];
        double time_ms;
        math_family_t detected;
        
        int psi_val = psi_solve_math_family(&kp, &tree, family_groups, &family_count,
                                            best, &time_ms, &detected);
        
        // Checkpoints
        for (int c = 0; c < 10; c++) {
            if (i + 1 == checkpoints[c]) {
                int hits = 0;
                for (int t = 0; t < 20; t++) {
                    knapsack_problem_t test_kp;
                    knapsack_generate(&test_kp, 200, 50, 200, 1000, t + 50000);
                    
                    int test_sel[MAX_ITEMS];
                    int test_dp = knapsack_dp_exact(&test_kp, test_sel);
                    
                    int test_best[MAX_ITEMS];
                    double test_time;
                    math_family_t test_family;
                    
                    int test_val = psi_solve_math_family(&test_kp, &tree, family_groups, 
                                                         &family_count, test_best, 
                                                         &test_time, &test_family);
                    if (test_val == test_dp) hits++;
                }
                learning_curve[c] = (double)hits / 20.0 * 100.0;
                
                printf("Checkpoint %d: %d/20 exact (%.1f%%), families: %d\n",
                       i + 1, hits, learning_curve[c], family_count);
            }
        }
    }
    
    printf("\n📊 MATH FAMILY GROUPS DISCOVERED:\n");
    const char *family_names[] = {"Linear", "Quadratic", "Exponential", "Logarithmic", "Unknown"};
    for (int i = 0; i < family_count; i++) {
        printf("   Family %d: %-12s | degree=%.2f | members=%d | success=%.1f%% | time=%.3fms\n",
               i, family_names[family_groups[i].family_type],
               family_groups[i].degree,
               family_groups[i].member_count,
               family_groups[i].avg_success_rate * 100,
               family_groups[i].avg_time_ms);
    }
    
    printf("\n🧪 PHASE 2: FAMILY-SPECIFIC TESTING\n");
    printf("──────────────────────────────────────────────────────\n");
    
    struct {
        int count;
        int max_w;
        int max_v;
        int capacity;
        int seed_offset;
        const char *name;
    } test_families[] = {
        {200, 50, 200, 1000, 20000, "A-test (similar)"},
        {200, 60, 180, 1200, 21000, "B-transfer"},
        {150, 80, 150, 800,  22000, "C-generalize"},
        {300, 30, 300, 1500, 23000, "D-novel"}
    };
    
    for (int fam = 0; fam < 4; fam++) {
        int exact = 0;
        double total_time = 0;
        math_family_t dominant_family = MATH_FAMILY_UNKNOWN;
        int family_counts[5] = {0};
        
        for (int t = 0; t < TEST_SIZE; t++) {
            knapsack_problem_t kp;
            knapsack_generate(&kp, test_families[fam].count, test_families[fam].max_w,
                             test_families[fam].max_v, test_families[fam].capacity,
                             test_families[fam].seed_offset + t);
            
            int selected[MAX_ITEMS];
            int dp_val = knapsack_dp_exact(&kp, selected);
            
            int best[MAX_ITEMS];
            double time_ms;
            math_family_t detected;
            
            int psi_val = psi_solve_math_family(&kp, &tree, family_groups, &family_count,
                                                best, &time_ms, &detected);
            
            if (psi_val == dp_val) exact++;
            total_time += time_ms;
            family_counts[detected]++;
        }
        
        // Find dominant family
        int max_count = 0;
        for (int f = 0; f < 5; f++) {
            if (family_counts[f] > max_count) {
                max_count = family_counts[f];
                dominant_family = (math_family_t)f;
            }
        }
        
        printf("\n📋 Family %s:\n", test_families[fam].name);
        printf("   Dominant math family: %s\n", family_names[dominant_family]);
        printf("   Exact hits:     %d/%d (%.1f%%)\n", exact, TEST_SIZE, (double)exact/TEST_SIZE*100);
        printf("   Avg time:       %.3f ms\n", total_time/TEST_SIZE);
        printf("   Family distribution: L=%d Q=%d E=%d Log=%d U=%d\n",
               family_counts[0], family_counts[1], family_counts[2], 
               family_counts[3], family_counts[4]);
    }
    
    printf("\n📈 LEARNING CURVE:\n");
    printf("   Problems | Exact Hit Rate | Math Families\n");
    printf("   ---------|----------------|---------------\n");
    for (int c = 0; c < 10; c++) {
        printf("   %8d | %.1f%%           | %d\n",
               checkpoints[c], learning_curve[c], c < family_count ? family_groups[c].member_count : 0);
    }
    
    printf("\n✅ Key Features:\n");
    printf("   • Auto-classification into mathematical families\n");
    printf("   • %d math family groups discovered\n", family_count);
    printf("   • Family-specific adaptive strategies\n");
    printf("   • Pattern tree with %d confirmed patterns\n", tree.confirmed_count);
    printf("   • %d learned compositions\n", tree.composition_count);
    
    return 0;
}
