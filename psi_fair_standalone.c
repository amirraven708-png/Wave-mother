#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define N_ITEMS   200
#define MAX_W     50
#define MAX_V     200
#define CAP       1000
#define INSTANCES 30

typedef struct { int w, v; } Item;
typedef struct { Item items[N_ITEMS]; int n, cap; } KP;

void eval(const KP *kp, const int *sel, int *val, int *wgt) {
    *val = *wgt = 0;
    for (int i = 0; i < kp->n; i++)
        if (sel[i]) { *val += kp->items[i].v; *wgt += kp->items[i].w; }
}

void gen(KP *kp, int seed) {
    srand(seed);
    kp->n = N_ITEMS; kp->cap = CAP;
    for (int i = 0; i < N_ITEMS; i++) {
        kp->items[i].w = 1 + rand() % MAX_W;
        kp->items[i].v = 1 + rand() % MAX_V;
    }
}

void greedy(const KP *kp, int *sel) {
    int ord[N_ITEMS]; double ratio[N_ITEMS];
    for (int i = 0; i < kp->n; i++) { ord[i] = i; ratio[i] = (double)kp->items[i].v / kp->items[i].w; }
    for (int i = 0; i < kp->n-1; i++)
        for (int j = i+1; j < kp->n; j++)
            if (ratio[ord[j]] > ratio[ord[i]]) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    memset(sel, 0, kp->n * sizeof(int));
    int w = 0;
    for (int i = 0; i < kp->n; i++) {
        int idx = ord[i];
        if (w + kp->items[idx].w <= kp->cap) { sel[idx] = 1; w += kp->items[idx].w; }
    }
}

int ls(const KP *kp, int *sel) {
    int bv, bw; eval(kp, sel, &bv, &bw);
    int improved = 0;
    for (int out = 0; out < kp->n; out++) {
        if (!sel[out]) continue;
        for (int in = 0; in < kp->n; in++) {
            if (sel[in]) continue;
            int nw = bw - kp->items[out].w + kp->items[in].w;
            if (nw > kp->cap) continue;
            int nv = bv - kp->items[out].v + kp->items[in].v;
            if (nv > bv) { sel[out] = 0; sel[in] = 1; bv = nv; bw = nw; improved = 1; }
        }
    }
    return improved;
}

void perturb(const KP *kp, int *sel, double N) {
    int flips = (int)(N * 0.5);
    if (flips < 1) flips = 1;
    if (flips > kp->n) flips = kp->n;
    for (int k = 0; k < flips; k++) {
        int idx = rand() % kp->n;
        sel[idx] = sel[idx] ? 0 : 1;
    }
    int val, wgt; eval(kp, sel, &val, &wgt);
    int guard = 0;
    while (wgt > kp->cap && guard < kp->n * 2) {
        int idx = rand() % kp->n;
        if (sel[idx]) { sel[idx] = 0; eval(kp, sel, &val, &wgt); }
        guard++;
    }
}

int main() {
    srand(12345);   // master seed for whole benchmark
    FILE *csv = fopen("psi_fair_learning.csv", "w");
    fprintf(csv, "seed,method,value,weight\n");

    double sum_grb = 0, sum_grls = 0, sum_psi = 0;
    int better = 0, equal = 0, worse = 0;

    for (int seed = 0; seed < INSTANCES; seed++) {
        KP kp; gen(&kp, seed);   // ← fresh problem for every seed

        int sel[N_ITEMS], val, wgt;

        // GR-B
        greedy(&kp, sel);
        eval(&kp, sel, &val, &wgt);
        sum_grb += val;
        fprintf(csv, "%d,GR-B,%d,%d\n", seed, val, wgt);

        // GR-LS
        while (ls(&kp, sel));
        eval(&kp, sel, &val, &wgt);
        int grls_val = val;
        sum_grls += val;
        fprintf(csv, "%d,GR-LS,%d,%d\n", seed, val, wgt);

        // PSI-CORE
        greedy(&kp, sel);
        while (ls(&kp, sel));
        eval(&kp, sel, &val, &wgt);
        int best_val = val, best_sel[N_ITEMS];
        memcpy(best_sel, sel, sizeof(sel));

        double residual = 1.0, coherence = 0.3, N_dim = 16.0;
        int no_imp = 0;
        for (int iter = 0; iter < 100; iter++) {
            double rp = residual;
            double da = 1.25 * (1.0 - coherence) * ((rp - residual > 0) ? 1.0 : 0.2);
            double db = 0.45 * (0.5 + coherence) * (1.0 + (1.0 - residual));
            double vel = da * residual - db * (N_dim - 3.0) + 0.20 * coherence;
            if (vel > 12) vel = 12; if (vel < -12) vel = -12;
            N_dim += vel * 0.5;
            if (N_dim < 3) N_dim = 3; if (N_dim > 200) N_dim = 200;

            memcpy(sel, best_sel, sizeof(sel));
            perturb(&kp, sel, N_dim);
            while (ls(&kp, sel));
            eval(&kp, sel, &val, &wgt);

            if (val > best_val) {
                best_val = val;
                memcpy(best_sel, sel, sizeof(sel));
                residual *= 0.85;
                coherence += 0.06; if (coherence > 1) coherence = 1;
                no_imp = 0;
            } else {
                residual += 0.03; if (residual > 1) residual = 1;
                coherence -= 0.025; if (coherence < 0) coherence = 0;
                no_imp++;
            }
            if (no_imp > 30) break;
        }
        sum_psi += best_val;
        fprintf(csv, "%d,PSI,%d,%d\n", seed, best_val, wgt);

        if (best_val > grls_val) better++;
        else if (best_val == grls_val) equal++;
        else worse++;
    }

    fclose(csv);

    printf("FAIR BENCHMARK (N=%d)\n", INSTANCES);
    printf("Method   | Avg Value\n");
    printf("GR-B     | %9.1f\n", sum_grb/INSTANCES);
    printf("GR-LS    | %9.1f\n", sum_grls/INSTANCES);
    printf("PSI-CORE | %9.1f\n", sum_psi/INSTANCES);
    printf("PSI vs GR-LS: better=%d  equal=%d  worse=%d\n", better, equal, worse);
    printf("Clean CSV: psi_fair_learning.csv\n");
    return 0;
}
