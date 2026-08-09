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
 * Local improvement: 1-for-1 swap (FIXED)
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
            
            // CRITICAL: Use current actual weight, not cached
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

