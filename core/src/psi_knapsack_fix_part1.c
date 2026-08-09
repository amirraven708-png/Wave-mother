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
 * NEW: Hard invariant validator
 * ------------------------------------------------------------ */
static int validate_solution(
    const knapsack_problem_t *kp,
    const int *selected)
{
    solution_stats_t s = evaluate(kp, selected);
    return s.weight <= kp->capacity;
}

/* ------------------------------------------------------------
 * NEW: Smart repair - removes worst ratio items first
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
 * FIXED: Perturbation with guaranteed feasibility
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
    
    // CRITICAL: Immediate repair after mutation
    repair_solution(kp, selected);
}

