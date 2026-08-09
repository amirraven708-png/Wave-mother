#ifndef PSI_EXPERIENCE_MEMORY_H
#define PSI_EXPERIENCE_MEMORY_H

#include <stdint.h>
#include <time.h>

#define MAX_EXPERIENCES 10000
#define MAX_PATTERNS 100
#define PATTERN_VECTOR_SIZE 5

typedef enum {
    FAMILY_A_TRAIN = 0,
    FAMILY_A_TEST = 1,
    FAMILY_B_TRANSFER = 2,
    FAMILY_C_GENERALIZE = 3,
    FAMILY_D_NOVEL = 4
} problem_family_t;

typedef struct {
    // Problem signature (NOT the solution)
    double avg_weight;
    double avg_value;
    double capacity_ratio;
    double item_count;
    double density;
    
    // Search context
    double initial_residual;
    double initial_coherence;
    double initial_N;
    
    // Strategy used
    double exploration_rate;
    double perturbation_intensity;
    int local_search_depth;
    
    // Outcome (learning signal, NOT answer)
    double improvement_rate;
    double repair_rate;
    double final_coherence;
    double convergence_speed;
    
    // Meta
    problem_family_t family;
    uint64_t timestamp;
    uint64_t reuse_count;
    double success_score;  // 0.0 to 1.0
    
} psi_experience_t;

typedef struct {
    double pattern_vector[PATTERN_VECTOR_SIZE];
    double avg_exploration_rate;
    double avg_perturbation;
    double success_rate;
    int experience_count;
    problem_family_t dominant_family;
} psi_pattern_t;

typedef struct {
    psi_experience_t experiences[MAX_EXPERIENCES];
    int experience_count;
    
    psi_pattern_t patterns[MAX_PATTERNS];
    int pattern_count;
    
    // Learning metrics
    int total_problems_seen;
    int total_successful_transfers;
    double cumulative_improvement;
    
    // Temporal tracking
    time_t start_time;
    time_t last_learning_event;
    int problems_since_last_learning;
    
} psi_memory_t;

// Memory management
void psi_memory_init(psi_memory_t *mem);
int psi_memory_store_experience(psi_memory_t *mem, const psi_experience_t *exp);
int psi_memory_find_similar_pattern(const psi_memory_t *mem, 
                                     const psi_experience_t *query,
                                     psi_pattern_t *matched_pattern,
                                     double *similarity_score);

// Learning transfer
double psi_memory_suggest_exploration_rate(const psi_memory_t *mem,
                                            const psi_experience_t *current_context);
double psi_memory_suggest_perturbation(const psi_memory_t *mem,
                                        const psi_experience_t *current_context);

// Pattern extraction
void psi_memory_extract_patterns(psi_memory_t *mem);
void psi_memory_print_learning_status(const psi_memory_t *mem);

#endif
