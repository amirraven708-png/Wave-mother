#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "psi_experience_memory.h"

void psi_memory_init(psi_memory_t *mem) {
    memset(mem, 0, sizeof(psi_memory_t));
    mem->start_time = time(NULL);
    mem->last_learning_event = mem->start_time;
}

static double calculate_pattern_similarity(const double *vec1, const double *vec2) {
    double dot = 0.0, mag1 = 0.0, mag2 = 0.0;
    for (int i = 0; i < PATTERN_VECTOR_SIZE; i++) {
        dot += vec1[i] * vec2[i];
        mag1 += vec1[i] * vec1[i];
        mag2 += vec2[i] * vec2[i];
    }
    if (mag1 < 0.0001 || mag2 < 0.0001) return 0.0;
    return dot / (sqrt(mag1) * sqrt(mag2));
}

static void extract_pattern_vector(const psi_experience_t *exp, double *vector) {
    vector[0] = exp->avg_weight / 100.0;      // Normalize
    vector[1] = exp->avg_value / 100.0;
    vector[2] = exp->capacity_ratio;
    vector[3] = exp->item_count / 100.0;
    vector[4] = exp->density * 10.0;
}

int psi_memory_store_experience(psi_memory_t *mem, const psi_experience_t *exp) {
    if (mem->experience_count >= MAX_EXPERIENCES) {
        // Simple FIFO: overwrite oldest
        mem->experiences[mem->experience_count % MAX_EXPERIENCES] = *exp;
        mem->experience_count++;
        return 1;
    }
    
    mem->experiences[mem->experience_count++] = *exp;
    mem->total_problems_seen++;
    mem->problems_since_last_learning++;
    
    // Extract patterns every 100 experiences
    if (mem->experience_count % 100 == 0) {
        psi_memory_extract_patterns(mem);
        mem->last_learning_event = time(NULL);
        mem->problems_since_last_learning = 0;
    }
    
    return 0;
}

int psi_memory_find_similar_pattern(const psi_memory_t *mem,
                                     const psi_experience_t *query,
                                     psi_pattern_t *matched_pattern,
                                     double *similarity_score) {
    if (mem->pattern_count == 0) return 0;
    
    double query_vec[PATTERN_VECTOR_SIZE];
    extract_pattern_vector(query, query_vec);
    
    double best_similarity = -1.0;
    int best_idx = -1;
    
    for (int i = 0; i < mem->pattern_count; i++) {
        double sim = calculate_pattern_similarity(query_vec, mem->patterns[i].pattern_vector);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_idx = i;
        }
    }
    
    if (best_idx >= 0 && best_similarity > 0.7) {  // Threshold
        *matched_pattern = mem->patterns[best_idx];
        *similarity_score = best_similarity;
        return 1;
    }
    
    return 0;
}

double psi_memory_suggest_exploration_rate(const psi_memory_t *mem,
                                            const psi_experience_t *current_context) {
    psi_pattern_t pattern;
    double similarity;
    
    if (psi_memory_find_similar_pattern(mem, current_context, &pattern, &similarity)) {
        // Weight by similarity and success rate
        return pattern.avg_exploration_rate * similarity * pattern.success_rate;
    }
    
    return -1.0;  // No suggestion
}

double psi_memory_suggest_perturbation(const psi_memory_t *mem,
                                        const psi_experience_t *current_context) {
    psi_pattern_t pattern;
    double similarity;
    
    if (psi_memory_find_similar_pattern(mem, current_context, &pattern, &similarity)) {
        return pattern.avg_perturbation * similarity;
    }
    
    return -1.0;  // No suggestion
}

void psi_memory_extract_patterns(psi_memory_t *mem) {
    // Simple clustering: group recent experiences by family
    int family_counts[5] = {0};
    double family_vectors[5][PATTERN_VECTOR_SIZE] = {{0}};
    double family_exploration[5] = {0};
    double family_perturbation[5] = {0};
    double family_success[5] = {0};
    int family_total[5] = {0};
    
    int start_idx = mem->experience_count > 500 ? mem->experience_count - 500 : 0;
    int count = mem->experience_count - start_idx;
    if (count > 500) count = 500;
    
    for (int i = 0; i < count; i++) {
        psi_experience_t *exp = &mem->experiences[start_idx + i];
        int fam = exp->family;
        if (fam > 4) fam = 4;
        
        double vec[PATTERN_VECTOR_SIZE];
        extract_pattern_vector(exp, vec);
        
        for (int j = 0; j < PATTERN_VECTOR_SIZE; j++) {
            family_vectors[fam][j] += vec[j];
        }
        
        family_exploration[fam] += exp->exploration_rate;
        family_perturbation[fam] += exp->perturbation_intensity;
        family_success[fam] += exp->success_score;
        family_total[fam]++;
    }
    
    // Create/update patterns
    mem->pattern_count = 0;
    for (int fam = 0; fam < 5; fam++) {
        if (family_total[fam] < 5) continue;  // Need minimum examples
        
        psi_pattern_t *pat = &mem->patterns[mem->pattern_count++];
        for (int j = 0; j < PATTERN_VECTOR_SIZE; j++) {
            pat->pattern_vector[j] = family_vectors[fam][j] / family_total[fam];
        }
        pat->avg_exploration_rate = family_exploration[fam] / family_total[fam];
        pat->avg_perturbation = family_perturbation[fam] / family_total[fam];
        pat->success_rate = family_success[fam] / family_total[fam];
        pat->experience_count = family_total[fam];
        pat->dominant_family = fam;
        
        if (mem->pattern_count >= MAX_PATTERNS) break;
    }
}

void psi_memory_print_learning_status(const psi_memory_t *mem) {
    time_t now = time(NULL);
    double hours_running = difftime(now, mem->start_time) / 3600.0;
    
    printf("   ⏱️  Running: %.1f hours\n", hours_running);
    printf("   📚 Experiences: %d\n", mem->experience_count);
    printf("   🧠 Patterns: %d\n", mem->pattern_count);
    printf("   📊 Problems seen: %d\n", mem->total_problems_seen);
    printf("   🎯 Successful transfers: %d\n", mem->total_successful_transfers);
    
    if (mem->total_problems_seen > 0) {
        double hours_since_last = difftime(now, mem->last_learning_event) / 3600.0;
        printf("   🔄 Last learning: %.1f hours ago (%d problems)\n", 
               hours_since_last, mem->problems_since_last_learning);
    }
    
    printf("   📈 Cumulative improvement: %.4f\n\n", mem->cumulative_improvement);
}
