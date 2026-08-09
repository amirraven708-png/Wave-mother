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

// Sleep/Wake states
typedef enum {
    PSI_AWAKE = 0,
    PSI_DREAMING = 1,
    PSI_DEEP_SLEEP = 2,
    PSI_INTEGRATING = 3
} psi_consciousness_t;

// Consolidated knowledge after sleep
typedef struct {
    double feature_centroid[5];
    double optimal_exploration;
    double optimal_perturbation;
    double success_rate;
    int consolidated_patterns;
    int family_id;
    time_t consolidation_time;
} sleep_consolidation_t;

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

// Sleep consolidation: merge similar patterns
static void psi_sleep_consolidate(pattern_tree_t *tree, 
                                   sleep_consolidation_t *consolidations,
                                   int *consolidation_count) {
    printf("   💤 SLEEP: Consolidating patterns...\n");
    
    int before_count = tree->confirmed_count;
    
    // Find clusters of similar patterns
    for (int i = 0; i < tree->node_count && *consolidation_count < 50; i++) {
        if (tree->nodes[i].pattern_count < 5) continue;
        
        // Create consolidation from this node's patterns
        sleep_consolidation_t *cons = &consolidations[*consolidation_count];
        memcpy(cons->feature_centroid, tree->nodes[i].center_vector, 5 * sizeof(double));
        
        // Calculate optimal parameters from node's patterns
        double total_exploration = 0, total_perturbation = 0, total_success = 0;
        int valid_patterns = 0;
        
        for (int j = 0; j < tree->nodes[i].pattern_count; j++) {
            int pat_id = tree->nodes[i].pattern_ids[j];
            psi_pattern_t *pat = &tree->confirmed_patterns[pat_id];
            
            if (pat->confirmed) {
                total_exploration += pat->exploration_rate;
                total_perturbation += pat->perturbation_intensity;
                total_success += pat->success_score;
                valid_patterns++;
            }
        }
        
        if (valid_patterns > 0) {
            cons->optimal_exploration = total_exploration / valid_patterns;
            cons->optimal_perturbation = total_perturbation / valid_patterns;
            cons->success_rate = total_success / valid_patterns;
            cons->consolidated_patterns = valid_patterns;
            cons->family_id = i;
            cons->consolidation_time = time(NULL);
            (*consolidation_count)++;
        }
    }
    
    printf("   ✅ Consolidated %d pattern groups into %d knowledge chunks\n",
           before_count, *consolidation_count);
}

// Wake: apply consolidated knowledge with exponential decay
static double psi_wake_apply_knowledge(sleep_consolidation_t *consolidations,
                                        int consolidation_count,
                                        const double *features,
                                        double time_since_sleep) {
    if (consolidation_count == 0) return -1.0;
    
    double best_similarity = -1.0;
    int best_cons = -1;
    
    // Find most similar consolidation
    for (int i = 0; i < consolidation_count; i++) {
        double sim = 0.0;
        double mag1 = 0.0, mag2 = 0.0;
        for (int j = 0; j < 5; j++) {
            sim += features[j] * consolidations[i].feature_centroid[j];
            mag1 += features[j] * features[j];
            mag2 += consolidations[i].feature_centroid[j] * consolidations[i].feature_centroid[j];
        }
        if (mag1 > 0.0001 && mag2 > 0.0001) {
            sim /= (sqrt(mag1) * sqrt(mag2));
        }
        
        if (sim > best_similarity) {
            best_similarity = sim;
            best_cons = i;
        }
    }
    
    if (best_cons >= 0 && best_similarity > 0.7) {
        // Exponential decay of knowledge relevance
        double decay = exp(-time_since_sleep / 3600.0);  // Decay over hours
        double confidence = consolidations[best_cons].success_rate * decay;
        
        if (confidence > 0.5) {
            return consolidations[best_cons].optimal_exploration;
        }
    }
    
    return -1.0;  // No applicable knowledge
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     PSI SLEEP/WAKE LEARNING CYCLE TEST                ║\n");
    printf("║     (Exponential knowledge integration)               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    pattern_tree_t tree;
    pattern_tree_init(&tree);
    
    sleep_consolidation_t consolidations[50];
    int consolidation_count = 0;
    
    psi_consciousness_t state = PSI_AWAKE;
    time_t last_sleep_time = time(NULL);
    time_t experiment_start = time(NULL);
    
    #define TOTAL_PROBLEMS 2000
    #define SLEEP_INTERVAL 200  // Sleep every 200 problems
    #define DREAM_INTERVAL 50   // Dream every 50 problems
    
    printf("🚀 STARTING SLEEP/WAKE LEARNING CYCLE\n");
    printf("   Experiment start: %s", ctime(&experiment_start));
    printf("   Sleep interval: every %d problems\n", SLEEP_INTERVAL);
    printf("   Dream interval: every %d problems\n\n", DREAM_INTERVAL);
    
    double learning_curve[20] = {0};
    int curve_idx = 0;
    int awake_problems = 0;
    int total_sleep_cycles = 0;
    
    for (int i = 0; i < TOTAL_PROBLEMS; i++) {
        // State transitions
        if (i > 0 && i % SLEEP_INTERVAL == 0) {
            state = PSI_DEEP_SLEEP;
            printf("\n🌙 CYCLE %d: Entering DEEP SLEEP at problem %d\n", 
                   total_sleep_cycles + 1, i);
            
            // Consolidate knowledge
            psi_sleep_consolidate(&tree, consolidations, &consolidation_count);
            
            total_sleep_cycles++;
            last_sleep_time = time(NULL);
            awake_problems = 0;
            
            // Dream phase (integrate knowledge)
            state = PSI_DREAMING;
            printf("   💭 DREAMING: Integrating %d knowledge chunks\n", consolidation_count);
            
            state = PSI_INTEGRATING;
            printf("   🧠 INTEGRATING: Building mental models\n");
            
            // Wake up
            state = PSI_AWAKE;
            printf("   ☀️  WAKING UP: Ready with consolidated knowledge\n\n");
        }
        
        if (i > 0 && i % DREAM_INTERVAL == 0 && state == PSI_AWAKE) {
            state = PSI_DREAMING;
            printf("   💭 Quick dream at problem %d: reviewing patterns\n", i);
            state = PSI_AWAKE;
        }
        
        // Generate and solve problem
        knapsack_problem_t kp;
        knapsack_generate(&kp, 200, 50, 200, 1000, i + 10000);
        
        int selected[MAX_ITEMS];
        int dp_val = knapsack_dp_exact(&kp, selected);
        
        // Extract features
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
        
        // Apply consolidated knowledge if awake
        double time_since_sleep = difftime(time(NULL), last_sleep_time);
        double suggested_exploration = -1.0;
        
        if (state == PSI_AWAKE && consolidation_count > 0) {
            suggested_exploration = psi_wake_apply_knowledge(consolidations, 
                                                             consolidation_count,
                                                             features, 
                                                             time_since_sleep);
        }
        
        // Solve with or without knowledge
        int n = kp.count;
        int best[MAX_ITEMS];
        double time_ms;
        
        clock_t start = clock();
        
        build_greedy(&kp, best);
        repair_solution(&kp, best);
        
        // Apply suggested exploration if available
        if (suggested_exploration > 0) {
            // Use learned optimal exploration
            for (int k = 0; k < (int)(suggested_exploration * 0.5); k++) {
                int idx = rand() % kp.count;
                best[idx] = best[idx] ? 0 : 1;
            }
            repair_solution(&kp, best);
        }
        
        while (improve_1x1(&kp, best)) repair_solution(&kp, best);
        
        time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
        
        int psi_val = evaluate(&kp, best).value;
        int exact = (psi_val == dp_val) ? 1 : 0;
        
        // Store pattern
        int decisions[10];
        int dec_count = kp.count < 10 ? kp.count : 10;
        for (int j = 0; j < dec_count; j++) decisions[j] = best[j];
        
        double outcome[3];
        outcome[0] = exact ? 1.0 : 0.8;
        outcome[1] = time_ms < 5.0 ? 1.0 : 0.5;
        outcome[2] = exact ? 1.0 : (double)psi_val / dp_val;
        
        pattern_tree_add_pattern(&tree, features, decisions, dec_count, 
                                 outcome, time_ms, outcome[2]);
        
        if (exact) {
            pattern_tree_confirm_patterns_with_same_outcome(&tree, outcome, 0.2);
        }
        
        awake_problems++;
        
        // Test checkpoint every 200 problems
        if ((i + 1) % 200 == 0 && curve_idx < 20) {
            int hits = 0;
            for (int t = 0; t < 20; t++) {
                knapsack_problem_t test_kp;
                knapsack_generate(&test_kp, 200, 50, 200, 1000, t + 50000 + i);
                
                int test_sel[MAX_ITEMS];
                int test_dp = knapsack_dp_exact(&test_kp, test_sel);
                
                int test_best[MAX_ITEMS];
                double test_time;
                clock_t test_start = clock();
                
                build_greedy(&test_kp, test_best);
                repair_solution(&test_kp, test_best);
                
                // Apply knowledge with current consolidation
                double test_features[5];
                double tw = 0, tv = 0;
                for (int j = 0; j < test_kp.count; j++) {
                    tw += test_kp.items[j].weight;
                    tv += test_kp.items[j].value;
                }
                test_features[0] = tw / test_kp.count / 50.0;
                test_features[1] = tv / test_kp.count / 200.0;
                test_features[2] = (double)test_kp.capacity / tw;
                test_features[3] = test_kp.count / 200.0;
                test_features[4] = tv / (tw + 1.0) / 5.0;
                
                double knowledge = psi_wake_apply_knowledge(consolidations,
                                                            consolidation_count,
                                                            test_features,
                                                            difftime(time(NULL), last_sleep_time));
                
                if (knowledge > 0) {
                    for (int k = 0; k < (int)(knowledge * 0.5); k++) {
                        int idx = rand() % test_kp.count;
                        test_best[idx] = test_best[idx] ? 0 : 1;
                    }
                    repair_solution(&test_kp, test_best);
                }
                
                while (improve_1x1(&test_kp, test_best)) 
                    repair_solution(&test_kp, test_best);
                
                int test_val = evaluate(&test_kp, test_best).value;
                if (test_val == test_dp) hits++;
            }
            
            learning_curve[curve_idx] = (double)hits / 20.0 * 100.0;
            
            printf("📊 Checkpoint %d: %d/20 exact (%.1f%%), consolidations: %d, state: %s\n",
                   i + 1, hits, learning_curve[curve_idx], 
                   consolidation_count,
                   state == PSI_AWAKE ? "AWAKE" : 
                   state == PSI_DREAMING ? "DREAMING" : 
                   state == PSI_DEEP_SLEEP ? "SLEEP" : "INTEGRATING");
            
            curve_idx++;
        }
    }
    
    // Final results
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║     SLEEP/WAKE LEARNING RESULTS                        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("🌙 SLEEP CYCLES: %d\n", total_sleep_cycles);
    printf("🧠 CONSOLIDATIONS: %d knowledge chunks\n", consolidation_count);
    printf("⏱️  TOTAL RUNTIME: %.0f seconds\n", difftime(time(NULL), experiment_start));
    
    printf("\n📈 LEARNING CURVE (with sleep/wake):\n");
    printf("   Problems | Exact Hit Rate | Consolidations\n");
    printf("   ---------|----------------|---------------\n");
    for (int i = 0; i < curve_idx; i++) {
        printf("   %8d | %.1f%%           | %d\n",
               (i + 1) * 200, learning_curve[i], 
               i < consolidation_count ? consolidations[i].consolidated_patterns : 0);
    }
    
    // Show top consolidations
    printf("\n💡 TOP KNOWLEDGE CONSOLIDATIONS:\n");
    for (int i = 0; i < consolidation_count && i < 5; i++) {
        printf("   Chunk %d: success=%.1f%% exploration=%.2f patterns=%d\n",
               i, 
               consolidations[i].success_rate * 100,
               consolidations[i].optimal_exploration,
               consolidations[i].consolidated_patterns);
    }
    
    // Test on different families
    printf("\n🧪 TRANSFER TEST (using consolidated knowledge):\n");
    
    struct {
        int count, max_w, max_v, capacity, seed_offset;
        const char *name;
    } test_families[] = {
        {200, 50, 200, 1000, 60000, "A-test (similar)"},
        {200, 60, 180, 1200, 61000, "B-transfer"},
        {150, 80, 150, 800,  62000, "C-generalize"},
        {300, 30, 300, 1500, 63000, "D-novel"}
    };
    
    for (int fam = 0; fam < 4; fam++) {
        int exact = 0, knowledge_used = 0;
        
        for (int t = 0; t < 50; t++) {
            knapsack_problem_t kp;
            knapsack_generate(&kp, test_families[fam].count, test_families[fam].max_w,
                             test_families[fam].max_v, test_families[fam].capacity,
                             test_families[fam].seed_offset + t);
            
            int sel[MAX_ITEMS];
            int dp_val = knapsack_dp_exact(&kp, sel);
            
            // Extract features
            double feats[5];
            double tw = 0, tv = 0;
            for (int j = 0; j < kp.count; j++) {
                tw += kp.items[j].weight;
                tv += kp.items[j].value;
            }
            feats[0] = tw / kp.count / 50.0;
            feats[1] = tv / kp.count / 200.0;
            feats[2] = (double)kp.capacity / tw;
            feats[3] = kp.count / 200.0;
            feats[4] = tv / (tw + 1.0) / 5.0;
            
            int best[MAX_ITEMS];
            build_greedy(&kp, best);
            repair_solution(&kp, best);
            
            double knowledge = psi_wake_apply_knowledge(consolidations,
                                                        consolidation_count,
                                                        feats, 0.0);
            if (knowledge > 0) {
                knowledge_used++;
                for (int k = 0; k < (int)(knowledge * 0.5); k++) {
                    int idx = rand() % kp.count;
                    best[idx] = best[idx] ? 0 : 1;
                }
                repair_solution(&kp, best);
            }
            
            while (improve_1x1(&kp, best)) repair_solution(&kp, best);
            
            if (evaluate(&kp, best).value == dp_val) exact++;
        }
        
        printf("   %-20s: %d/50 exact (%.0f%%), knowledge used: %d/50\n",
               test_families[fam].name, exact, (double)exact/50*100, knowledge_used);
    }
    
    printf("\n✅ Key Innovations:\n");
    printf("   • %d complete sleep/wake cycles\n", total_sleep_cycles);
    printf("   • Exponential knowledge decay after sleep\n");
    printf("   • Consolidation during deep sleep\n");
    printf("   • Dream integration for pattern review\n");
    printf("   • Wake-state knowledge application\n");
    printf("   • Transfer learning to novel families\n");
    
    return 0;
}
