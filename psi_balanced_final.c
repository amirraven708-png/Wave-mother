#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_ITEMS   200
#define CAPACITY    1000
#define MAX_W       50
#define MAX_V       200
#define ELITE_SIZE  10
#define INSTANCES   30

typedef struct { int w, v; } Item;
typedef struct { Item items[MAX_ITEMS]; int n, cap; } KP;
typedef struct { int sel[MAX_ITEMS]; int val, wgt; } Sol;
typedef struct {
    double N, coherence, residual, alpha, beta, gamma;
} PSI;
typedef struct { Sol elites[ELITE_SIZE]; int cnt; } Ellipse;

/* ---------- evaluation ---------- */
void eval(const KP *kp, Sol *s) {
    s->val = s->wgt = 0;
    for (int i = 0; i < kp->n; i++)
        if (s->sel[i]) { s->val += kp->items[i].v; s->wgt += kp->items[i].w; }
}

/* ---------- problem generator ---------- */
void gen(KP *kp, int seed) {
    srand(seed);
    kp->n = MAX_ITEMS; kp->cap = CAPACITY;
    for (int i = 0; i < MAX_ITEMS; i++) {
        kp->items[i].w = 1 + rand() % MAX_W;
        kp->items[i].v = 1 + rand() % MAX_V;
    }
}

/* ---------- greedy ---------- */
void greedy(const KP *kp, Sol *s) {
    int ord[MAX_ITEMS]; double ratio[MAX_ITEMS];
    for (int i = 0; i < kp->n; i++) { ord[i] = i; ratio[i] = (double)kp->items[i].v / kp->items[i].w; }
    for (int i = 0; i < kp->n-1; i++)
        for (int j = i+1; j < kp->n; j++)
            if (ratio[ord[j]] > ratio[ord[i]]) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    memset(s->sel, 0, sizeof(s->sel));
    int w = 0;
    for (int i = 0; i < kp->n; i++) {
        int idx = ord[i];
        if (w + kp->items[idx].w <= kp->cap) { s->sel[idx] = 1; w += kp->items[idx].w; }
    }
    eval(kp, s);
}

/* ---------- LOCAL SEARCH – FIXED ---------- */
void local_search(const KP *kp, Sol *s) {
    int improved = 1;
    while (improved) {
        improved = 0;
        for (int out = 0; out < kp->n; out++) {
            if (!s->sel[out]) continue;
            for (int in = 0; in < kp->n; in++) {
                if (s->sel[in]) continue;
                int nw = s->wgt - kp->items[out].w + kp->items[in].w;
                if (nw > kp->cap) continue;
                int nv = s->val - kp->items[out].v + kp->items[in].v;
                if (nv > s->val) {
                    s->sel[out] = 0; s->sel[in] = 1;
                    s->wgt = nw; s->val = nv;
                    improved = 1;
                    break;
                }
            }
            if (improved) break;
        }
    }
}

/* ---------- smart repair ---------- */
void repair(const KP *kp, Sol *s) {
    eval(kp, s);
    while (s->wgt > kp->cap) {
        int worst = -1; double minR = 1e9;
        for (int i = 0; i < kp->n; i++) {
            if (s->sel[i]) {
                double r = (double)kp->items[i].v / kp->items[i].w;
                if (r < minR) { minR = r; worst = i; }
            }
        }
        if (worst >= 0) s->sel[worst] = 0; else break;
        eval(kp, s);
    }
}

/* ---------- balanced wave engine (NO FUZZY) ---------- */
void balanced_wave(const KP *kp, PSI *psi, Sol *s, Ellipse *mem) {
    // ALWAYS use local search as a base, then perturb only when coherence is low
    Sol cur = *s;
    
    // Always apply local search first (exploitation)
    local_search(kp, &cur);
    
    // If coherence is very high and residual low, just keep best
    if (psi->coherence > 0.85 && psi->residual < 0.15) {
        *s = cur;
        return;
    }
    
    // Perturbation: much gentler
    int flips = (int)(psi->N * 0.1);  // was 0.4 or 0.8 – now only 10%
    if (flips < 1) flips = 1;
    if (flips > kp->n) flips = kp->n;
    
    // Store original to revert if perturbation fails
    Sol orig = cur;
    
    for (int k = 0; k < flips; k++) {
        int idx = rand() % kp->n;
        cur.sel[idx] = !cur.sel[idx];
    }
    repair(kp, &cur);
    local_search(kp, &cur);
    
    // If perturbation made things worse, keep original
    if (cur.val < orig.val) {
        cur = orig;
        // penalize: increase residual, decrease coherence
        psi->residual += 0.05;
        psi->coherence -= 0.03;
    } else if (cur.val > orig.val) {
        // reward: decrease residual, increase coherence
        psi->residual *= 0.9;
        psi->coherence += 0.04;
    }
    
    // Clamp
    if (psi->coherence > 1.0) psi->coherence = 1.0;
    if (psi->coherence < 0.0) psi->coherence = 0.0;
    if (psi->residual > 1.0) psi->residual = 1.0;
    if (psi->residual < 0.01) psi->residual = 0.01;
    
    psi->N = psi->alpha * psi->residual - psi->beta * psi->coherence + psi->gamma;
    if (psi->N < 3) psi->N = 3;
    if (psi->N > 200) psi->N = 200;
    
    *s = cur;
}

/* ---------- main benchmark ---------- */
int main() {
    srand(12345);
    KP kp;
    PSI psi = {16.0, 0.3, 1.0, 1.25, 0.45, 0.20};
    Ellipse mem; mem.cnt = 0;

    FILE *log = fopen("psi_balanced_final_learning.csv", "w");
    fprintf(log, "instance,method,value,weight\n");

    double sum_greedy = 0, sum_ls = 0, sum_psi = 0;
    int better = 0, equal = 0, worse = 0;

    for (int seed = 0; seed < INSTANCES; seed++) {
        gen(&kp, seed);
        Sol sol;

        // Greedy
        greedy(&kp, &sol);
        sum_greedy += sol.val;
        fprintf(log, "%d,greedy,%d,%d\n", seed, sol.val, sol.wgt);

        // Greedy + Local Search (FIXED)
        Sol sol_ls = sol;
        local_search(&kp, &sol_ls);
        sum_ls += sol_ls.val;
        fprintf(log, "%d,ls,%d,%d\n", seed, sol_ls.val, sol_ls.wgt);

        // PSI-Core (50 iterations)
        Sol sol_psi = sol_ls;
        for (int iter = 0; iter < 50; iter++) {
            balanced_wave(&kp, &psi, &sol_psi, &mem);
        }
        sum_psi += sol_psi.val;
        fprintf(log, "%d,psi,%d,%d\n", seed, sol_psi.val, sol_psi.wgt);

        if (sol_psi.val > sol_ls.val) better++;
        else if (sol_psi.val == sol_ls.val) equal++;
        else worse++;
    }

    fclose(log);

    printf("BALANCED FINAL BENCHMARK (capacity=%d, instances=%d)\n", CAPACITY, INSTANCES);
    printf("Greedy    avg: %.1f\n", sum_greedy/INSTANCES);
    printf("LS        avg: %.1f\n", sum_ls/INSTANCES);
    printf("PSI-CORE  avg: %.1f\n", sum_psi/INSTANCES);
    printf("PSI vs LS: better=%d  equal=%d  worse=%d\n", better, equal, worse);
    printf("Clean CSV: psi_balanced_final_learning.csv\n");
    return 0;
}
