/* ------------------------------------------------------------
 * Trajectory logging
 * ------------------------------------------------------------ */
static void log_trajectory(
    FILE *fp,
    int iteration,
    double residual,
    double delta_residual,
    double coherence,
    double delta_coherence,
    double N,
    int value,
    int weight,
    int capacity,
    int feasible,
    double feasibility_score,
    double progress_score)
{
    fprintf(fp,
        "%d,%.6f,%.6f,%.6f,%.6f,%.3f,%d,%d,%d,%d,%.3f,%.3f\n",
        iteration,
        residual,
        delta_residual,
        coherence,
        delta_coherence,
        N,
        value,
        weight,
        capacity,
        feasible,
        feasibility_score,
        progress_score);
}

/* ------------------------------------------------------------
 * NEW: Real residual calculation
 * ------------------------------------------------------------ */
static double calculate_residual(
    int weight,
    int capacity,
    int current_value,
    int best_value,
    double old_residual)
{
    // Feasibility component (dominant)
    double R_feasibility = 0.0;
    if (weight > capacity) {
        R_feasibility = (double)(weight - capacity) / capacity;
        if (R_feasibility > 1.0) R_feasibility = 1.0;
    }
    
    // Progress component
    double R_progress = 0.0;
    if (best_value > 0) {
        R_progress = 1.0 - (double)current_value / best_value;
        if (R_progress < 0.0) R_progress = 0.0;
    }
    
    // Stability component (smooth changes)
    double R_stability = old_residual;
    
    // Weighted combination
    double residual = 0.6 * R_feasibility + 0.3 * R_progress + 0.1 * R_stability;
    
    if (residual < 0.0) residual = 0.0;
    if (residual > 1.0) residual = 1.0;
    
    return residual;
}

/* ------------------------------------------------------------
 * NEW: Real coherence calculation
 * ------------------------------------------------------------ */
static double calculate_coherence(
    int weight,
    int capacity,
    int candidate_value,
    int best_value,
    double delta_value,
    double old_coherence)
{
    // Feasibility (binary)
    double feasibility = (weight <= capacity) ? 1.0 : 0.0;
    
    // Progress score
    double progress_score = 0.0;
    if (candidate_value > best_value) {
        progress_score = 1.0;
    } else if (candidate_value == best_value) {
        progress_score = 0.5;
    }
    
    // Stability
    double stability = exp(-fabs(delta_value) / (fabs(best_value) + 1.0));
    
    // Weighted combination
    double coherence = 0.45 * feasibility + 0.35 * progress_score + 0.20 * stability;
    
    // Smooth with history
    coherence = 0.7 * coherence + 0.3 * old_coherence;
    
    if (coherence < 0.0) coherence = 0.0;
    if (coherence > 1.0) coherence = 1.0;
    
    return coherence;
}

/* ------------------------------------------------------------
 * FIXED: PSI solver with hard invariants
 * ------------------------------------------------------------ */
static int knapsack_psi_solve(
    const knapsack_problem_t *kp,
    int *selected,
    psi_core_engine_t *psi,
    FILE *logfile)
{
    int n = kp->count;
    int current[MAX_ITEMS];
    int best[MAX_ITEMS];
    
    // Initial feasible solution
    build_greedy(kp, current);
    repair_solution(kp, current);
    
    while (improve_1x1(kp, current))
        repair_solution(kp, current);
    
    solution_stats_t current_stats = evaluate(kp, current);
    memcpy(best, current, n * sizeof(int));
    solution_stats_t best_stats = current_stats;
    
    // Initialize with real observable values
    double residual = calculate_residual(best_stats.weight, kp->capacity, 
                                         best_stats.value, best_stats.value, 0.3);
    double coherence = calculate_coherence(best_stats.weight, kp->capacity,
                                          best_stats.value, best_stats.value, 0.0, 0.3);
    
    double previous_best = (double)best_stats.value;
    int max_iters = 100;
    
    for (int iter = 0; iter < max_iters; iter++) {
        double old_residual = residual;
        double old_coherence = coherence;
        
        // PSI decides exploration intensity
        psi_core_breathe(psi, residual, coherence, 0.25);
        double N = psi->dimensional_state;
        
        // Create candidate from best
        memcpy(current, best, n * sizeof(int));
        
        // Explore
        psi_perturb(kp, current, N);
        repair_solution(kp, current);  // CRITICAL: Always repair
        
        // Exploit
        while (improve_1x1(kp, current))
            repair_solution(kp, current);  // CRITICAL: Always repair
        
        solution_stats_t candidate = evaluate(kp, current);
        
        // HARD INVARIANT CHECK
        if (!validate_solution(kp, current)) {
            fprintf(stderr, 
                "PSI INVARIANT VIOLATION: iter=%d weight=%d capacity=%d\n",
                iter, candidate.weight, kp->capacity);
            repair_solution(kp, current);
            candidate = evaluate(kp, current);
            
            if (!validate_solution(kp, current)) {
                fprintf(stderr, "FATAL: Cannot repair solution, skipping\n");
                continue;
            }
        }
        
        double improvement = (double)candidate.value - previous_best;
        
        // Update best if improved AND feasible
        if (candidate.value > best_stats.value && validate_solution(kp, current)) {
            memcpy(best, current, n * sizeof(int));
            best_stats = candidate;
            previous_best = (double)best_stats.value;
        }
        
        // Calculate REAL residual and coherence
        residual = calculate_residual(
            candidate.weight, kp->capacity,
            candidate.value, best_stats.value,
            residual);
        
        coherence = calculate_coherence(
            candidate.weight, kp->capacity,
            candidate.value, best_stats.value,
            improvement, coherence);
        
        // Log trajectory with feasibility flag
        log_trajectory(
            logfile,
            iter,
            residual,
            residual - old_residual,
            coherence,
            coherence - old_coherence,
            N,
            best_stats.value,
            best_stats.weight,
            kp->capacity,
            validate_solution(kp, best) ? 1 : 0,
            (best_stats.weight <= kp->capacity) ? 1.0 : 0.0,
            (improvement > 0.0) ? 1.0 : 0.0);
        
        // Stopping criteria based on real metrics
        if (residual < 0.05 && coherence > 0.9 && 
            validate_solution(kp, best) && best_stats.weight <= kp->capacity) {
            break;
        }
    }
    
    // FINAL CHECK
    if (!validate_solution(kp, best)) {
        fprintf(stderr, "WARNING: Best solution still infeasible, repairing\n");
        repair_solution(kp, best);
        best_stats = evaluate(kp, best);
    }
    
    memcpy(selected, best, n * sizeof(int));
    return best_stats.value;
}

