#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

/* ========== KNAPSACK CORE ========== */
#define MAX_ITEMS 2000
typedef struct { int weight, value; } Item;
typedef struct { Item items[MAX_ITEMS]; int count, capacity; } KP;

static KP generate_kp(int n, int max_w, int max_v, int cap, unsigned int seed) {
    srand(seed);
    KP kp; kp.count = n; kp.capacity = cap;
    for (int i = 0; i < n; i++) {
        kp.items[i].weight = 1 + rand() % max_w;
        kp.items[i].value  = 1 + rand() % max_v;
    }
    return kp;
}

/* ---------- Exact DP ---------- */
static int dp_exact(const KP *kp, int *sel) {
    int n = kp->count, W = kp->capacity;
    int **dp = malloc((n+1) * sizeof(int*));
    for (int i=0;i<=n;i++) dp[i] = calloc(W+1, sizeof(int));
    for (int i=1;i<=n;i++)
        for (int w=0;w<=W;w++) {
            dp[i][w] = dp[i-1][w];
            if (kp->items[i-1].weight <= w) {
                int t = dp[i-1][w - kp->items[i-1].weight] + kp->items[i-1].value;
                if (t > dp[i][w]) dp[i][w] = t;
            }
        }
    int w = W;
    memset(sel, 0, n * sizeof(int));
    for (int i=n;i>0;i--)
        if (dp[i][w] != dp[i-1][w]) { sel[i-1] = 1; w -= kp->items[i-1].weight; }
    int res = dp[n][W];
    for (int i=0;i<=n;i++) free(dp[i]); free(dp);
    return res;
}

/* ---------- Greedy ---------- */
static int greedy(const KP *kp, int *sel) {
    int n = kp->count;
    double ratio[MAX_ITEMS]; int idx[MAX_ITEMS];
    for (int i=0;i<n;i++) { ratio[i] = (double)kp->items[i].value/kp->items[i].weight; idx[i]=i; }
    for (int i=0;i<n-1;i++)
        for (int j=i+1;j<n;j++)
            if (ratio[idx[j]] > ratio[idx[i]]) { int t=idx[i]; idx[i]=idx[j]; idx[j]=t; }
    memset(sel,0,n*sizeof(int));
    int w=0, v=0;
    for (int i=0;i<n;i++) {
        int id = idx[i];
        if (w + kp->items[id].weight <= kp->capacity) {
            sel[id]=1; w += kp->items[id].weight; v += kp->items[id].value;
        }
    }
    return v;
}

/* ---------- Local Search (1x1 swap) ---------- */
static int local_search(const KP *kp, int *sel) {
    int improved=1, n=kp->count;
    while (improved) {
        improved=0;
        int cur_w=0, cur_v=0;
        for (int i=0;i<n;i++) if (sel[i]) { cur_w += kp->items[i].weight; cur_v += kp->items[i].value; }
        for (int out=0;out<n;out++) {
            if (!sel[out]) continue;
            for (int in=0;in<n;in++) {
                if (sel[in]) continue;
                int nw = cur_w - kp->items[out].weight + kp->items[in].weight;
                if (nw > kp->capacity) continue;
                int nv = cur_v - kp->items[out].value + kp->items[in].value;
                if (nv > cur_v) {
                    sel[out]=0; sel[in]=1;
                    cur_w = nw; cur_v = nv;
                    improved=1;
                }
            }
        }
    }
    int v=0; for (int i=0;i<n;i++) if (sel[i]) v += kp->items[i].value;
    return v;
}

/* ---------- Random feasible ---------- */
static int random_feasible(const KP *kp, int *sel) {
    int n=kp->count, w=0, v=0;
    memset(sel,0,n*sizeof(int));
    int order[MAX_ITEMS]; for (int i=0;i<n;i++) order[i]=i;
    for (int i=0;i<n-1;i++) { int j = i + rand()%(n-i); int t=order[i]; order[i]=order[j]; order[j]=t; }
    for (int i=0;i<n;i++) {
        int id = order[i];
        if (w + kp->items[id].weight <= kp->capacity) {
            sel[id]=1; w += kp->items[id].weight; v += kp->items[id].value;
        }
    }
    return v;
}

/* ---------- Repair utility ---------- */
static void repair(const KP *kp, int *sel) {
    int n = kp->count, w = 0;
    for (int i = 0; i < n; i++) if (sel[i]) w += kp->items[i].weight;
    while (w > kp->capacity) {
        int worst = -1; double worst_ratio = 1e30;
        for (int i = 0; i < n; i++) {
            if (!sel[i]) continue;
            double r = (double)kp->items[i].value / kp->items[i].weight;
            if (r < worst_ratio) { worst_ratio = r; worst = i; }
        }
        if (worst < 0) break;
        w -= kp->items[worst].weight;
        sel[worst] = 0;
    }
}

/* ---------- Perturb ---------- */
static void perturb(const KP *kp, int *sel, double N) {
    int n = kp->count;
    int attempts = (int)(N * 0.5); if (attempts < 1) attempts = 1; if (attempts > n) attempts = n;
    for (int k=0;k<attempts;k++) { int idx = rand()%n; sel[idx] = !sel[idx]; }
    repair(kp, sel);
}

/* ========== PSI-CORE ENGINE ========== */
typedef struct {
    double dim_state, alpha_base, beta_base, gamma;
    double prev_residual, prev_coherence, coherence;
} PSI;

static void psi_init(PSI *p) {
    p->dim_state = 16.0; p->alpha_base = 1.25; p->beta_base = 0.45; p->gamma = 0.20;
    p->prev_residual = 1.0; p->prev_coherence = 0.0; p->coherence = 0.0;
}

static void psi_breathe(PSI *p, double residual, double coherence, double dt) {
    if (residual < 0) residual = 0; if (residual > 1) residual = 1;
    if (coherence < 0) coherence = 0; if (coherence > 1) coherence = 1;
    double progress = p->prev_residual - residual;
    double factor = progress > 0 ? 1.0 : 0.20;
    double dyn_alpha = p->alpha_base * (1.0 - coherence) * factor;
    double dyn_beta  = p->beta_base * (0.5 + coherence) * (1.0 + (1.0 - residual));
    double expansion = dyn_alpha * residual;
    double contraction = dyn_beta * (p->dim_state - 3.0);
    double resonance = p->gamma * coherence;
    double vel = expansion - contraction + resonance;
    if (vel > 12) vel = 12; if (vel < -12) vel = -12;
    p->dim_state += vel * dt;
    if (p->dim_state < 3.0) p->dim_state = 3.0;
    if (p->dim_state > 200.0) p->dim_state = 200.0;
    p->prev_residual = residual;
    p->prev_coherence = p->coherence;
    p->coherence = coherence;
}

/* ========== SOLVERS ========== */

/* Baseline: Greedy + LS */
static int greedy_ls(const KP *kp, int *sel) {
    greedy(kp, sel);
    return local_search(kp, sel);
}

/* PSI-Core */
static int psi_solver(const KP *kp, int *best_sel, PSI *psi, int max_iter) {
    int n = kp->count;
    int *cur = malloc(n*sizeof(int));
    greedy_ls(kp, cur);
    int *best = malloc(n*sizeof(int)); memcpy(best, cur, n*sizeof(int));
    int best_val = 0; for (int i=0;i<n;i++) if (best[i]) best_val += kp->items[i].value;
    double residual = 0.1, coherence = 0.5;
    int prev_best = best_val;
    for (int iter=0; iter < max_iter; iter++) {
        psi_breathe(psi, residual, coherence, 0.25);
        double N = psi->dim_state;
        memcpy(cur, best, n*sizeof(int));
        perturb(kp, cur, N);
        local_search(kp, cur);
        int cur_val = 0; for (int i=0;i<n;i++) if (cur[i]) cur_val += kp->items[i].value;
        double improvement = cur_val - prev_best;
        if (cur_val > best_val) {
            memcpy(best, cur, n*sizeof(int)); best_val = cur_val; prev_best = best_val;
            residual *= 0.82; if (residual < 0.01) residual = 0.01;
        } else {
            residual += 0.035; if (residual > 1.0) residual = 1.0;
        }
        if (improvement > 0) coherence += 0.08; else coherence -= 0.025;
        if (coherence > 1) coherence = 1; if (coherence < 0) coherence = 0;
        double stability = exp(-fabs(residual - psi->prev_residual) * 8.0);
        coherence = 0.85*coherence + 0.15*stability;
        if (residual < 0.015 && coherence > 0.95) break;
    }
    memcpy(best_sel, best, n*sizeof(int));
    free(cur); free(best);
    return best_val;
}

/* WAKE: large perturbation when stagnating */
static int wake_solver(const KP *kp, int *best_sel, int max_iter) {
    int n = kp->count;
    int *cur = malloc(n*sizeof(int));
    greedy_ls(kp, cur);
    int *best = malloc(n*sizeof(int)); memcpy(best, cur, n*sizeof(int));
    int best_val = 0; for (int i=0;i<n;i++) if (best[i]) best_val += kp->items[i].value;
    int stagnate = 0;
    for (int iter=0; iter < max_iter; iter++) {
        double N = (stagnate > 5) ? 30.0 : 8.0;  // WAKE: large perturbation when stuck
        memcpy(cur, best, n*sizeof(int));
        perturb(kp, cur, N);
        local_search(kp, cur);
        int cur_val = 0; for (int i=0;i<n;i++) if (cur[i]) cur_val += kp->items[i].value;
        if (cur_val > best_val) {
            memcpy(best, cur, n*sizeof(int)); best_val = cur_val;
            stagnate = 0;
        } else {
            stagnate++;
        }
    }
    memcpy(best_sel, best, n*sizeof(int));
    free(cur); free(best);
    return best_val;
}

/* SLEEP: small perturbation for refinement */
static int sleep_solver(const KP *kp, int *best_sel, int max_iter) {
    int n = kp->count;
    int *cur = malloc(n*sizeof(int));
    greedy_ls(kp, cur);
    int *best = malloc(n*sizeof(int)); memcpy(best, cur, n*sizeof(int));
    int best_val = 0; for (int i=0;i<n;i++) if (best[i]) best_val += kp->items[i].value;
    for (int iter=0; iter < max_iter; iter++) {
        double N = 4.0;  // SLEEP: small, careful perturbation
        memcpy(cur, best, n*sizeof(int));
        perturb(kp, cur, N);
        local_search(kp, cur);
        int cur_val = 0; for (int i=0;i<n;i++) if (cur[i]) cur_val += kp->items[i].value;
        if (cur_val > best_val) {
            memcpy(best, cur, n*sizeof(int)); best_val = cur_val;
        }
    }
    memcpy(best_sel, best, n*sizeof(int));
    free(cur); free(best);
    return best_val;
}

/* DUAL: PSI with corrected residual calculation (before best update) */
static int dual_solver(const KP *kp, int *best_sel, PSI *psi, int max_iter) {
    int n = kp->count;
    int *cur = malloc(n*sizeof(int));
    greedy_ls(kp, cur);
    int *best = malloc(n*sizeof(int)); memcpy(best, cur, n*sizeof(int));
    int best_val = 0; for (int i=0;i<n;i++) if (best[i]) best_val += kp->items[i].value;
    double residual = 0.3, coherence = 0.3;
    int prev_best = best_val;
    for (int iter=0; iter < max_iter; iter++) {
        psi_breathe(psi, residual, coherence, 0.25);
        double N = psi->dim_state;
        memcpy(cur, best, n*sizeof(int));
        perturb(kp, cur, N);
        local_search(kp, cur);
        int cur_val = 0, cur_w = 0;
        for (int i=0;i<n;i++) if (cur[i]) { cur_val += kp->items[i].value; cur_w += kp->items[i].weight; }
        
        // Calculate residuals BEFORE updating best (prevents zeroing)
        double Rf = (cur_w > kp->capacity) ? (double)(cur_w - kp->capacity)/kp->capacity : 0;
        if (Rf > 1) Rf = 1;
        double Rp = (prev_best > 0) ? 1.0 - (double)cur_val/prev_best : 0;
        if (Rp < 0) Rp = 0;
        residual = 0.6*Rf + 0.3*Rp + 0.1*residual;
        if (residual < 0) residual = 0; if (residual > 1) residual = 1;
        
        double improvement = cur_val - prev_best;
        if (cur_val > best_val) {
            memcpy(best, cur, n*sizeof(int)); best_val = cur_val;
        }
        prev_best = best_val;  // track best so far for next residual
        
        double feas = (cur_w <= kp->capacity) ? 1.0 : 0.0;
        double prog = (improvement > 0) ? 1.0 : ((improvement == 0) ? 0.5 : 0.0);
        double stab = exp(-fabs(improvement) / (fabs((double)best_val)+1.0));
        coherence = 0.7*(0.45*feas + 0.35*prog + 0.20*stab) + 0.3*coherence;
        if (coherence < 0) coherence = 0; if (coherence > 1) coherence = 1;
    }
    memcpy(best_sel, best, n*sizeof(int));
    free(cur); free(best);
    return best_val;
}

/* Adaptive PSI-Core: switches between WAKE and SLEEP based on DUAL feedback */
static int adaptive_psi_solver(const KP *kp, int *best_sel, PSI *psi, int max_iter) {
    int n = kp->count;
    int *cur = malloc(n*sizeof(int));
    greedy_ls(kp, cur);
    int *best = malloc(n*sizeof(int)); memcpy(best, cur, n*sizeof(int));
    int best_val = 0; for (int i=0;i<n;i++) if (best[i]) best_val += kp->items[i].value;
    double residual = 0.3, coherence = 0.3;
    int prev_best = best_val;
    int stagnate = 0;
    for (int iter=0; iter < max_iter; iter++) {
        psi_breathe(psi, residual, coherence, 0.25);
        double base_N = psi->dim_state;
        // Adaptive: if stagnating, boost exploration (WAKE), else exploit (SLEEP)
        double N = (stagnate > 5) ? base_N * 2.0 : base_N * 0.5;
        if (N < 3) N = 3; if (N > 200) N = 200;
        
        memcpy(cur, best, n*sizeof(int));
        perturb(kp, cur, N);
        local_search(kp, cur);
        int cur_val = 0, cur_w = 0;
        for (int i=0;i<n;i++) if (cur[i]) { cur_val += kp->items[i].value; cur_w += kp->items[i].weight; }
        
        double Rf = (cur_w > kp->capacity) ? (double)(cur_w - kp->capacity)/kp->capacity : 0;
        if (Rf > 1) Rf = 1;
        double Rp = (prev_best > 0) ? 1.0 - (double)cur_val/prev_best : 0;
        if (Rp < 0) Rp = 0;
        residual = 0.6*Rf + 0.3*Rp + 0.1*residual;
        
        double improvement = cur_val - prev_best;
        if (cur_val > best_val) {
            memcpy(best, cur, n*sizeof(int)); best_val = cur_val;
            stagnate = 0;
        } else {
            stagnate++;
        }
        prev_best = best_val;
        
        double feas = (cur_w <= kp->capacity) ? 1.0 : 0.0;
        double prog = (improvement > 0) ? 1.0 : ((improvement == 0) ? 0.5 : 0.0);
        double stab = exp(-fabs(improvement) / (fabs((double)best_val)+1.0));
        coherence = 0.7*(0.45*feas + 0.35*prog + 0.20*stab) + 0.3*coherence;
    }
    memcpy(best_sel, best, n*sizeof(int));
    free(cur); free(best);
    return best_val;
}

/* ========== BENCHMARK INFRASTRUCTURE ========== */
typedef struct {
    const char *name;
    int (*solver)(const KP*, int*);
    double times[1000];
    double values[1000];
    double gaps[1000];
    int feas[1000];
    int exact[1000];
    int count;
    double avg_time, avg_value, avg_gap, avg_feas;
    double best_gap, worst_gap, median_gap, std_gap;
    double hit_rate;
    double stability; // 1 - (std_gap / avg_gap) or similar
} MethodStats;

static void compute_stats(MethodStats *m) {
    int n = m->count;
    double sum_t=0, sum_v=0, sum_g=0, sum_f=0;
    double best_g=1e30, worst_g=-1e30;
    int hit=0;
    for (int i=0;i<n;i++) {
        sum_t += m->times[i]; sum_v += m->values[i]; sum_g += m->gaps[i]; sum_f += m->feas[i];
        if (m->gaps[i] < best_g) best_g = m->gaps[i];
        if (m->gaps[i] > worst_g) worst_g = m->gaps[i];
        if (m->exact[i]) hit++;
    }
    m->avg_time = sum_t/n;
    m->avg_value = sum_v/n;
    m->avg_gap = sum_g/n;
    m->avg_feas = sum_f/n * 100;
    m->best_gap = best_g;
    m->worst_gap = worst_g;
    m->hit_rate = (double)hit/n * 100;
    
    // median and std
    double *g = malloc(n*sizeof(double));
    memcpy(g, m->gaps, n*sizeof(double));
    for (int i=0;i<n-1;i++)
        for (int j=i+1;j<n;j++)
            if (g[j] < g[i]) { double t=g[i]; g[i]=g[j]; g[j]=t; }
    m->median_gap = g[n/2];
    double mean = m->avg_gap;
    double var = 0;
    for (int i=0;i<n;i++) var += (m->gaps[i]-mean)*(m->gaps[i]-mean);
    m->std_gap = sqrt(var/n);
    m->stability = (m->std_gap < 0.001) ? 1.0 : 1.0 / (1.0 + m->std_gap);
    free(g);
}

static double time_diff_ms(clock_t start) { return (double)(clock()-start)/CLOCKS_PER_SEC*1000.0; }

/* ========== MAIN BENCHMARK ========== */
int main() {
    const int sizes[] = {50, 200, 500};
    const int num_seeds = 30;
    const int max_iter = 50;
    const char *methods[] = {"Greedy+LS", "Random", "PSI-Core", "WAKE", "SLEEP", "DUAL", "AdaptivePSI"};
    const int num_methods = 7;

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          WAVE MOTHER FINAL BENCHMARK v2.0                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    // Store all method stats per size
    MethodStats all_stats[3][num_methods];
    memset(all_stats, 0, sizeof(all_stats));

    for (int si=0; si<3; si++) {
        int n = sizes[si];
        int cap = n * 10;
        printf("━━━ Size n=%d, capacity=%d, seeds=%d ━━━\n", n, cap, num_seeds);
        
        MethodStats stats[num_methods];
        for (int m=0; m<num_methods; m++) {
            stats[m].name = methods[m];
            stats[m].count = 0;
        }
        
        for (int seed=1; seed<=num_seeds; seed++) {
            KP kp = generate_kp(n, 50, 200, cap, seed*1000 + si*100);
            int sel[MAX_ITEMS];
            int dp = dp_exact(&kp, sel);
            
            // 1. Greedy+LS
            int idx = 0;
            clock_t t0 = clock();
            int val = greedy_ls(&kp, sel);
            double t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            
            // 2. Random
            idx = 1;
            t0 = clock();
            val = random_feasible(&kp, sel);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            
            // 3. PSI-Core
            idx = 2;
            PSI psi; psi_init(&psi);
            int *sol = malloc(n*sizeof(int));
            t0 = clock();
            val = psi_solver(&kp, sol, &psi, max_iter);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            free(sol);
            
            // 4. WAKE
            idx = 3;
            sol = malloc(n*sizeof(int));
            t0 = clock();
            val = wake_solver(&kp, sol, max_iter);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            free(sol);
            
            // 5. SLEEP
            idx = 4;
            sol = malloc(n*sizeof(int));
            t0 = clock();
            val = sleep_solver(&kp, sol, max_iter);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            free(sol);
            
            // 6. DUAL
            idx = 5;
            PSI psi2; psi_init(&psi2);
            sol = malloc(n*sizeof(int));
            t0 = clock();
            val = dual_solver(&kp, sol, &psi2, max_iter);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            free(sol);
            
            // 7. Adaptive PSI
            idx = 6;
            PSI psi3; psi_init(&psi3);
            sol = malloc(n*sizeof(int));
            t0 = clock();
            val = adaptive_psi_solver(&kp, sol, &psi3, max_iter);
            t = time_diff_ms(t0);
            stats[idx].times[stats[idx].count] = t;
            stats[idx].values[stats[idx].count] = val;
            stats[idx].gaps[stats[idx].count] = (double)(dp-val)/dp*100;
            stats[idx].exact[stats[idx].count] = (val==dp);
            stats[idx].feas[stats[idx].count] = 1;
            stats[idx].count++;
            free(sol);
        }
        
        // Compute statistics for all methods
        for (int m=0; m<num_methods; m++) {
            compute_stats(&stats[m]);
            all_stats[si][m] = stats[m];
        }
        
        // Print table
        printf("%-14s %8s %8s %8s %8s %8s %8s\n", "Method", "AvgGap%", "Best%", "Worst%", "Hit%", "Time(ms)", "Stab");
        printf("────────────── ──────── ──────── ──────── ──────── ──────── ────────\n");
        for (int m=0; m<num_methods; m++) {
            printf("%-14s %8.2f %8.2f %8.2f %7.1f %8.2f %7.2f\n",
                   stats[m].name, stats[m].avg_gap, stats[m].best_gap, stats[m].worst_gap,
                   stats[m].hit_rate, stats[m].avg_time, stats[m].stability);
        }
        printf("\n");
    }
    
    // Overall winners across all sizes (aggregate)
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    OVERALL WINNERS                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Aggregate metrics across all sizes
    double total_avg_gap[7] = {0}, total_hit[7] = {0}, total_time[7] = {0}, total_stab[7] = {0};
    for (int m=0; m<num_methods; m++) {
        for (int si=0; si<3; si++) {
            total_avg_gap[m] += all_stats[si][m].avg_gap;
            total_hit[m] += all_stats[si][m].hit_rate;
            total_time[m] += all_stats[si][m].avg_time;
            total_stab[m] += all_stats[si][m].stability;
        }
    }
    
    // Determine winners
    int best_quality = 0, best_speed = 0, best_stability = 0, best_tradeoff = 0;
    double min_gap = 1e30, min_time = 1e30, max_stab = -1e30, best_trade = -1e30;
    
    for (int m=0; m<num_methods; m++) {
        if (total_avg_gap[m] < min_gap) { min_gap = total_avg_gap[m]; best_quality = m; }
        if (total_time[m] < min_time) { min_time = total_time[m]; best_speed = m; }
        if (total_stab[m] > max_stab) { max_stab = total_stab[m]; best_stability = m; }
        // Trade-off: hit rate / time (higher is better)
        double trade = (100.0 - total_avg_gap[m]) / (total_time[m] + 0.001);
        if (trade > best_trade) { best_trade = trade; best_tradeoff = m; }
    }
    
    printf("QUALITY WINNER      : %s (avg gap %.2f%%)\n", methods[best_quality], total_avg_gap[best_quality]/3);
    printf("SPEED WINNER        : %s (avg time %.2f ms)\n", methods[best_speed], total_time[best_speed]/3);
    printf("STABILITY WINNER    : %s (stability %.3f)\n", methods[best_stability], total_stab[best_stability]/3);
    printf("BEST TRADE-OFF      : %s (score %.1f)\n", methods[best_tradeoff], best_trade);
    printf("EXACT HIT WINNER    : %s (hit rate %.1f%%)\n", methods[best_quality], total_hit[best_quality]/3);
    
    return 0;
}
