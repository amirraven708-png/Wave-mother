#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "wave_multi_agent.h"
#include "wave_psi_core_v2.h"

#define INSTANCES 30

typedef struct { int value, weight; } sol_stats_t;

static sol_stats_t evaluate(const knapsack_problem_t *kp, const int *sel) {
    sol_stats_t s = {0,0};
    for (int i = 0; i < kp->count; i++) {
        if (sel[i]) { s.value += kp->items[i].value; s.weight += kp->items[i].weight; }
    }
    return s;
}

static void build_greedy(const knapsack_problem_t *kp, int *sel) {
    int n = kp->count, order[MAX_ITEMS]; double ratio[MAX_ITEMS];
    for (int i = 0; i < n; i++) { order[i]=i; ratio[i]=(double)kp->items[i].value/kp->items[i].weight; }
    for (int i = 0; i < n-1; i++) for (int j = i+1; j < n; j++)
        if (ratio[order[j]] > ratio[order[i]]) { int t=order[i]; order[i]=order[j]; order[j]=t; }
    memset(sel, 0, n*sizeof(int));
    int w=0;
    for (int i=0;i<n;i++) { int idx=order[i]; if (w+kp->items[idx].weight <= kp->capacity) { sel[idx]=1; w+=kp->items[idx].weight; } }
}

static int improve_1x1(const knapsack_problem_t *kp, int *sel) {
    sol_stats_t best = evaluate(kp,sel); int improved=0;
    for (int out=0; out<kp->count; out++) {
        if (!sel[out]) continue;
        for (int in=0; in<kp->count; in++) {
            if (sel[in]) continue;
            int nw = best.weight - kp->items[out].weight + kp->items[in].weight;
            if (nw > kp->capacity) continue;
            int nv = best.value - kp->items[out].value + kp->items[in].value;
            if (nv > best.value) { sel[out]=0; sel[in]=1; best.weight=nw; best.value=nv; improved=1; }
        }
    }
    return improved;
}

static void psi_perturb(const knapsack_problem_t *kp, int *sel, double N) {
    int attempts = (int)(N*0.5); if(attempts<1) attempts=1; if(attempts>kp->count) attempts=kp->count;
    for (int k=0;k<attempts;k++) { int idx=rand()%kp->count; sel[idx]=sel[idx]?0:1; }
    // --- SAFETY CHECK: force feasibility after perturbation ---
    sol_stats_t s = evaluate(kp,sel);
    int guard=0;
    while(s.weight > kp->capacity && guard < kp->count*2) {
        int idx = rand()%kp->count;
        if(sel[idx]) { sel[idx]=0; s = evaluate(kp,sel); }
        guard++;
    }
}

static double resilience_score(const knapsack_problem_t *kp, const int *sel, int value) {
    int n=kp->count; int noisy[MAX_ITEMS]; memcpy(noisy,sel,n*sizeof(int));
    for (int i=0;i<50;i++) { int idx=rand()%n; noisy[idx]=noisy[idx]?0:1; }
    sol_stats_t s = evaluate(kp,noisy);
    int guard=0;
    while(s.weight > kp->capacity && guard < kp->count*2) {
        int idx=rand()%n; if(noisy[idx]) { noisy[idx]=0; s=evaluate(kp,noisy); }
        guard++;
    }
    return (s.value >= value) ? 1.0 : 0.0;
}

int main() {
    srand(12345);   // master seed
    int n_items=200, max_w=50, max_v=200, cap=1000;

    FILE *csv = fopen("logs/psi_fair_learning.csv","w");
    fprintf(csv,"seed,method,value,weight,improvements,sum_N,resilience\n");

    double grb_sum=0, grls_sum=0, psi_sum=0;
    double grb_res=0, grls_res=0, psi_res=0;
    double grb_imp=0, grls_imp=0, psi_imp=0;
    double grb_N=0, grls_N=0, psi_N=0;
    int psi_better=0, psi_equal=0, psi_worse=0;

    for (int seed=0; seed<INSTANCES; seed++) {
        srand(seed);   // fresh problem for each seed
        knapsack_problem_t kp;
        knapsack_generate(&kp, n_items, max_w, max_v, cap);
        int n = kp.count;
        int sel[MAX_ITEMS];
        sol_stats_t s;

        // GR-B
        build_greedy(&kp, sel);
        s = evaluate(&kp, sel);
        int grb_val = s.value;
        grb_sum += grb_val;
        grb_res += resilience_score(&kp, sel, grb_val);
        fprintf(csv, "%d,GR-B,%d,%d,0,0.0,%.1f\n", seed, grb_val, s.weight, resilience_score(&kp,sel,grb_val));

        // GR-LS
        int imp_ls=0;
        while(improve_1x1(&kp,sel)) imp_ls++;
        s = evaluate(&kp,sel);
        int grls_val = s.value;
        grls_sum += grls_val;
        grls_imp += imp_ls;
        grls_res += resilience_score(&kp,sel,grls_val);
        fprintf(csv, "%d,GR-LS,%d,%d,%d,0.0,%.1f\n", seed, grls_val, s.weight, imp_ls, resilience_score(&kp,sel,grls_val));

        // PSI-CORE
        psi_core_engine_t psi;
        psi_core_init(&psi);
        double residual=1.0, coherence=0.3;
        int best_sel[MAX_ITEMS], cur[MAX_ITEMS];
        build_greedy(&kp, best_sel);
        s = evaluate(&kp, best_sel);
        int psi_val = s.value;
        int psi_improvements=0;
        double psi_sum_N=0.0;
        int no_improve=0;
        for (int iter=0; iter<100; iter++) {
            psi_core_breathe(&psi, residual, coherence, 0.5);
            double N = psi.dimensional_state;
            psi_sum_N += N;
            memcpy(cur, best_sel, n*sizeof(int));
            psi_perturb(&kp, cur, N);
            int ls=0;
            while(improve_1x1(&kp, cur)) ls++;
            s = evaluate(&kp, cur);
            if (s.value > psi_val) {
                psi_val = s.value;
                memcpy(best_sel, cur, n*sizeof(int));
                psi_improvements++;
                residual *= 0.85;
                coherence += 0.06;
                if(coherence>1.0) coherence=1.0;
                no_improve=0;
            } else {
                residual += 0.03;
                if(residual>1.0) residual=1.0;
                coherence -= 0.025;
                if(coherence<0.0) coherence=0.0;
                no_improve++;
            }
            if(no_improve>30) break;
        }
        psi_sum += psi_val;
        psi_imp += psi_improvements;
        psi_N += psi_sum_N;
        psi_res += resilience_score(&kp, best_sel, psi_val);
        fprintf(csv, "%d,PSI,%d,%d,%d,%.1f,%.1f\n", seed, psi_val, evaluate(&kp,best_sel).weight, psi_improvements, psi_sum_N, resilience_score(&kp,best_sel,psi_val));

        if (psi_val > grls_val) psi_better++;
        else if (psi_val == grls_val) psi_equal++;
        else psi_worse++;
    }

    fclose(csv);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   FAIR BENCHMARK (N=%d instances)            ║\n", INSTANCES);
    printf("╚══════════════════════════════════════════════╝\n\n");
    printf("Method     | Avg Value | Avg Imp | Avg sum_N | Resilience\n");
    printf("───────────┼───────────┼─────────┼───────────┼───────────\n");
    printf("GR-B       | %9.1f | %7.1f | %9.1f | %9.3f\n", grb_sum/INSTANCES, grb_imp/INSTANCES, grb_N/INSTANCES, grb_res/INSTANCES);
    printf("GR-LS      | %9.1f | %7.1f | %9.1f | %9.3f\n", grls_sum/INSTANCES, grls_imp/INSTANCES, grls_N/INSTANCES, grls_res/INSTANCES);
    printf("PSI-CORE   | %9.1f | %7.1f | %9.1f | %9.3f\n", psi_sum/INSTANCES, psi_imp/INSTANCES, psi_N/INSTANCES, psi_res/INSTANCES);
    printf("───────────┴───────────┴─────────┴───────────┴───────────\n");
    printf("PSI vs GR-LS : better=%d  equal=%d  worse=%d\n", psi_better, psi_equal, psi_worse);
    printf("\n✅ Clean CSV: logs/psi_fair_learning.csv\n");
    return 0;
}
