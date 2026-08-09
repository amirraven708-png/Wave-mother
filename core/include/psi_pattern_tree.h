#ifndef PSI_PATTERN_TREE_H
#define PSI_PATTERN_TREE_H

#include <stdint.h>
#include <time.h>

#define MAX_TREE_NODES 10000
#define MAX_PATTERNS_PER_NODE 50
#define MAX_COMPOSITIONS 200
#define MAX_PATH_LENGTH 20

typedef enum {
    NODE_ROOT = 0,
    NODE_FEATURE = 1,
    NODE_STRATEGY = 2,
    NODE_OUTCOME = 3
} node_type_t;

typedef struct {
    // Problem signature (مبدا)
    double feature_vector[5];  // avg_weight, avg_value, capacity_ratio, item_count, density
    
    // Decision path (مسیر)
    int decisions[10];  // Sequence of choices
    int decision_count;
    
    // Outcome signature (مقصد/برایند)
    double outcome_vector[3];  // success_rate, time_efficiency, quality_score
    
    // Validation
    int confirmed;  // آیا پترن تأیید شده؟
    int confirmation_count;
    double confidence;
    
    // Performance
    double avg_time_ms;
    double avg_quality;
    int usage_count;
    
    uint64_t timestamp;
} psi_pattern_t;

typedef struct {
    int pattern_ids[MAX_PATTERNS_PER_NODE];
    int pattern_count;
    node_type_t type;
    double center_vector[5];  // Centroid of patterns in this node
} tree_node_t;

typedef struct {
    psi_pattern_t confirmed_patterns[MAX_TREE_NODES];
    int confirmed_count;
    
    tree_node_t nodes[MAX_TREE_NODES];
    int node_count;
    
    // Composition memory (ترکیب‌های موفق)
    struct {
        int pattern_sequence[MAX_PATH_LENGTH];
        int sequence_length;
        double combined_success_rate;
        double combined_time_ms;
        int usage_count;
    } compositions[MAX_COMPOSITIONS];
    int composition_count;
    
} pattern_tree_t;

// Tree operations
void pattern_tree_init(pattern_tree_t *tree);
int pattern_tree_add_pattern(pattern_tree_t *tree, 
                              const double *features,
                              const int *decisions,
                              int decision_count,
                              const double *outcome,
                              double time_ms,
                              double quality);

// Pattern matching
int pattern_tree_find_matching_patterns(const pattern_tree_t *tree,
                                         const double *features,
                                         int *matched_ids,
                                         int max_matches,
                                         double *similarities);

// Composition (ترکیب پترن‌ها)
int pattern_tree_compose_solution(const pattern_tree_t *tree,
                                   const double *target_features,
                                   const double *target_outcome,
                                   int *pattern_sequence,
                                   int *sequence_length,
                                   double *estimated_time);

// Confirmation (تأیید پترن)
void pattern_tree_confirm_pattern(pattern_tree_t *tree, int pattern_id);
void pattern_tree_confirm_patterns_with_same_outcome(pattern_tree_t *tree, 
                                                       const double *outcome,
                                                       double tolerance);

// Shortest path finding
int pattern_tree_find_shortest_path(const pattern_tree_t *tree,
                                     const double *start_features,
                                     const double *target_outcome,
                                     int *best_sequence,
                                     int *sequence_length);

// Learning
void pattern_tree_learn_composition(pattern_tree_t *tree,
                                     const int *sequence,
                                     int length,
                                     double success,
                                     double time_ms);

// Statistics
void pattern_tree_print_stats(const pattern_tree_t *tree);

#endif
