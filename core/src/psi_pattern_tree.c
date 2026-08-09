#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "psi_pattern_tree.h"

void pattern_tree_init(pattern_tree_t *tree) {
    memset(tree, 0, sizeof(pattern_tree_t));
    
    // Create root node
    tree->nodes[0].type = NODE_ROOT;
    tree->nodes[0].center_vector[0] = 0.0;
    tree->nodes[0].center_vector[1] = 0.0;
    tree->nodes[0].center_vector[2] = 0.0;
    tree->nodes[0].center_vector[3] = 0.0;
    tree->nodes[0].center_vector[4] = 0.0;
    tree->node_count = 1;
}

static double vector_similarity(const double *v1, const double *v2, int dim) {
    double dot = 0.0, mag1 = 0.0, mag2 = 0.0;
    for (int i = 0; i < dim; i++) {
        dot += v1[i] * v2[i];
        mag1 += v1[i] * v1[i];
        mag2 += v2[i] * v2[i];
    }
    if (mag1 < 0.0001 || mag2 < 0.0001) return 0.0;
    return dot / (sqrt(mag1) * sqrt(mag2));
}

int pattern_tree_add_pattern(pattern_tree_t *tree,
                              const double *features,
                              const int *decisions,
                              int decision_count,
                              const double *outcome,
                              double time_ms,
                              double quality) {
    if (tree->confirmed_count >= MAX_TREE_NODES) return -1;
    
    psi_pattern_t *pat = &tree->confirmed_patterns[tree->confirmed_count];
    memcpy(pat->feature_vector, features, 5 * sizeof(double));
    memcpy(pat->decisions, decisions, decision_count * sizeof(int));
    pat->decision_count = decision_count;
    memcpy(pat->outcome_vector, outcome, 3 * sizeof(double));
    pat->confirmed = 0;  // Not confirmed yet
    pat->confirmation_count = 0;
    pat->confidence = 0.5;
    pat->avg_time_ms = time_ms;
    pat->avg_quality = quality;
    pat->usage_count = 1;
    pat->timestamp = time(NULL);
    
    // Add to appropriate node
    int best_node = 0;  // Start from root
    double best_similarity = -1.0;
    
    for (int i = 0; i < tree->node_count; i++) {
        double sim = vector_similarity(features, tree->nodes[i].center_vector, 5);
        if (sim > best_similarity && tree->nodes[i].pattern_count < MAX_PATTERNS_PER_NODE) {
            best_similarity = sim;
            best_node = i;
        }
    }
    
    // Create new node if no good match
    if (best_similarity < 0.7 && tree->node_count < MAX_TREE_NODES) {
        best_node = tree->node_count++;
        tree->nodes[best_node].type = NODE_FEATURE;
        memcpy(tree->nodes[best_node].center_vector, features, 5 * sizeof(double));
    }
    
    int node_idx = tree->nodes[best_node].pattern_count++;
    tree->nodes[best_node].pattern_ids[node_idx] = tree->confirmed_count;
    
    return tree->confirmed_count++;
}

int pattern_tree_find_matching_patterns(const pattern_tree_t *tree,
                                         const double *features,
                                         int *matched_ids,
                                         int max_matches,
                                         double *similarities) {
    int found = 0;
    
    for (int i = 0; i < tree->node_count && found < max_matches; i++) {
        double sim = vector_similarity(features, tree->nodes[i].center_vector, 5);
        
        if (sim > 0.6) {  // Similarity threshold
            for (int j = 0; j < tree->nodes[i].pattern_count && found < max_matches; j++) {
                int pat_id = tree->nodes[i].pattern_ids[j];
                if (tree->confirmed_patterns[pat_id].confirmed) {
                    matched_ids[found] = pat_id;
                    similarities[found] = sim;
                    found++;
                }
            }
        }
    }
    
    // Sort by similarity (bubble sort for simplicity)
    for (int i = 0; i < found - 1; i++) {
        for (int j = i + 1; j < found; j++) {
            if (similarities[j] > similarities[i]) {
                double tmp_sim = similarities[i];
                similarities[i] = similarities[j];
                similarities[j] = tmp_sim;
                
                int tmp_id = matched_ids[i];
                matched_ids[i] = matched_ids[j];
                matched_ids[j] = tmp_id;
            }
        }
    }
    
    return found;
}

void pattern_tree_confirm_pattern(pattern_tree_t *tree, int pattern_id) {
    if (pattern_id < 0 || pattern_id >= tree->confirmed_count) return;
    
    psi_pattern_t *pat = &tree->confirmed_patterns[pattern_id];
    pat->confirmation_count++;
    
    if (pat->confirmation_count >= 3) {
        pat->confirmed = 1;
        pat->confidence = 0.9;
    } else {
        pat->confidence = 0.5 + pat->confirmation_count * 0.15;
    }
}

void pattern_tree_confirm_patterns_with_same_outcome(pattern_tree_t *tree,
                                                       const double *outcome,
                                                       double tolerance) {
    int confirmed_batch = 0;
    
    for (int i = 0; i < tree->confirmed_count; i++) {
        if (tree->confirmed_patterns[i].confirmed) continue;
        
        double diff = 0.0;
        for (int j = 0; j < 3; j++) {
            diff += fabs(tree->confirmed_patterns[i].outcome_vector[j] - outcome[j]);
        }
        
        if (diff < tolerance) {
            pattern_tree_confirm_pattern(tree, i);
            confirmed_batch++;
        }
    }
    
    if (confirmed_batch > 0) {
        printf("   ✓ Batch confirmed %d patterns with similar outcomes\n", confirmed_batch);
    }
}

int pattern_tree_compose_solution(const pattern_tree_t *tree,
                                   const double *target_features,
                                   const double *target_outcome,
                                   int *pattern_sequence,
                                   int *sequence_length,
                                   double *estimated_time) {
    // Find individual patterns
    int candidates[50];
    double similarities[50];
    int num_candidates = pattern_tree_find_matching_patterns(tree, target_features,
                                                              candidates, 50, similarities);
    
    if (num_candidates == 0) return 0;
    
    // Try single pattern first (کوتاه‌ترین مسیر = یک پترن)
    for (int i = 0; i < num_candidates; i++) {
        psi_pattern_t *pat = &tree->confirmed_patterns[candidates[i]];
        double outcome_sim = vector_similarity(target_outcome, pat->outcome_vector, 3);
        
        if (outcome_sim > 0.9) {
            pattern_sequence[0] = candidates[i];
            *sequence_length = 1;
            *estimated_time = pat->avg_time_ms;
            return 1;
        }
    }
    
    // Try compositions (ترکیب پترن‌ها)
    int best_comp = -1;
    double best_time = 1e30;
    
    for (int c = 0; c < tree->composition_count; c++) {
        double outcome_sim = vector_similarity(target_outcome, 
                                               tree->confirmed_patterns[
                                                   tree->compositions[c].pattern_sequence[0]
                                               ].outcome_vector, 3);
        
        if (outcome_sim > 0.8 && tree->compositions[c].combined_time_ms < best_time) {
            best_comp = c;
            best_time = tree->compositions[c].combined_time_ms;
        }
    }
    
    if (best_comp >= 0) {
        memcpy(pattern_sequence, tree->compositions[best_comp].pattern_sequence,
               tree->compositions[best_comp].sequence_length * sizeof(int));
        *sequence_length = tree->compositions[best_comp].sequence_length;
        *estimated_time = tree->compositions[best_comp].combined_time_ms;
        return 1;
    }
    
    // Fallback: use best single pattern
    pattern_sequence[0] = candidates[0];
    *sequence_length = 1;
    *estimated_time = tree->confirmed_patterns[candidates[0]].avg_time_ms;
    return 1;
}

int pattern_tree_find_shortest_path(const pattern_tree_t *tree,
                                     const double *start_features,
                                     const double *target_outcome,
                                     int *best_sequence,
                                     int *sequence_length) {
    double estimated_time;
    return pattern_tree_compose_solution(tree, start_features, target_outcome,
                                         best_sequence, sequence_length, &estimated_time);
}

void pattern_tree_learn_composition(pattern_tree_t *tree,
                                     const int *sequence,
                                     int length,
                                     double success,
                                     double time_ms) {
    if (tree->composition_count >= MAX_COMPOSITIONS) return;
    if (length < 2) return;  // Only learn compositions of 2+ patterns
    
    // Check if this composition already exists
    for (int c = 0; c < tree->composition_count; c++) {
        if (tree->compositions[c].sequence_length == length) {
            int match = 1;
            for (int i = 0; i < length; i++) {
                if (tree->compositions[c].pattern_sequence[i] != sequence[i]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                // Update existing composition
                tree->compositions[c].usage_count++;
                tree->compositions[c].combined_success_rate = 
                    (tree->compositions[c].combined_success_rate * 0.9) + (success * 0.1);
                tree->compositions[c].combined_time_ms = 
                    (tree->compositions[c].combined_time_ms * 0.9) + (time_ms * 0.1);
                return;
            }
        }
    }
    
    // New composition
    int idx = tree->composition_count++;
    memcpy(tree->compositions[idx].pattern_sequence, sequence, length * sizeof(int));
    tree->compositions[idx].sequence_length = length;
    tree->compositions[idx].combined_success_rate = success;
    tree->compositions[idx].combined_time_ms = time_ms;
    tree->compositions[idx].usage_count = 1;
}

void pattern_tree_print_stats(const pattern_tree_t *tree) {
    printf("🧠 Pattern Tree Statistics:\n");
    printf("   Total patterns: %d\n", tree->confirmed_count);
    printf("   Confirmed patterns: ");
    
    int confirmed = 0;
    for (int i = 0; i < tree->confirmed_count; i++) {
        if (tree->confirmed_patterns[i].confirmed) confirmed++;
    }
    printf("%d (%.1f%%)\n", confirmed, 
           tree->confirmed_count > 0 ? (double)confirmed/tree->confirmed_count*100 : 0);
    
    printf("   Tree nodes: %d\n", tree->node_count);
    printf("   Learned compositions: %d\n", tree->composition_count);
    
    if (tree->composition_count > 0) {
        printf("   Avg composition length: %.1f\n", 
               (double)tree->composition_count / tree->composition_count);
    }
    
    printf("   Node distribution:\n");
    for (int i = 0; i < tree->node_count && i < 5; i++) {
        printf("      Node %d: %d patterns, center=[%.2f,%.2f,%.2f,%.2f,%.2f]\n",
               i, tree->nodes[i].pattern_count,
               tree->nodes[i].center_vector[0],
               tree->nodes[i].center_vector[1],
               tree->nodes[i].center_vector[2],
               tree->nodes[i].center_vector[3],
               tree->nodes[i].center_vector[4]);
    }
    if (tree->node_count > 5) {
        printf("      ... and %d more nodes\n", tree->node_count - 5);
    }
}
