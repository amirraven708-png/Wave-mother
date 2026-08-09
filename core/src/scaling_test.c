#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 5000
#define INSTANCES 20

typedef struct { int value, weight; } item_t;
typedef struct { item_t items[MAX_ITEMS]; int count, capacity; } knapsack_t;
typedef struct { int value, weight, is_valid; } result_t;

result_t evaluate(const knapsack_t *kp, const int *sel) {
    result_t r = {0, 0, 0};
    for (int i = 0; i < kp->count; i++) {
        if (sel[i]) { r.value += kp->items[i].value; r.weight += kp->items[i].weight; }
    }
    r.is_valid = (r.weight <= kp->capacity) ? 1 : 0;
    return r;
}

void run_greedy(const knapsack_t *kp, int *sel) {
    int order[MAX_ITEMS]; double ratio[MAX_ITEMS];
    for (int i = 0; i < kp->count; i++) {
        order[i] = i; ratio[i] = (double)kp->items[i].value / kp->items[i].weight;
    }
    for (int i = 0; i < kp->count - 1; i++) {
        for (int j = i + 1; j < kp->count; j++) {
            if (ratio[order[j]] > ratio[order[i]]) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }
    memset(sel, 0, kp->count * sizeof(int));
    int current_weight = 0;
    for (int i = 0; i < kp->count; i++) {
        int idx = order[i];
        if (current_weight + kp->items[idx].weight <= kp->capacity) {
            sel[idx] = 1; current_weight += kp->items[idx].weight;
        }
    }
}

// Same fix as before: break after an accepted swap to keep the invariant valid.
void run_local_search(const knapsack_t *kp, int *sel) {
    result_t best = evaluate(kp, sel);
    int improved = 1;
    while (improved) {
        improved = 0;
        for (int out = 0; out < kp->count; out++) {
            if (!sel[out]) continue;
            for (int in = 0; in < kp->count; in++) {
                if (sel[in]) continue;
                int nw = best.weight - kp->items[out].weight + kp->items[in].weight;
                if (nw > kp->capacity) continue;
                int nv = best.value - kp->items[out].value + kp->items[in].value;
                if (nv > best.value) {
                    sel[out] = 0; sel[in] = 1;
                    best.weight = nw; best.value = nv; improved = 1;
                    break;
                }
            }
        }
    }
}

void run_psi(const knapsack_t *kp, int *sel) {
    result_t best = evaluate(kp, sel);
    int *current = malloc(kp->count * sizeof(int));
    int no_improve = 0;
    double N_dim = 1.0;

    for (int iter = 0; iter < 100 && no_improve < 30; iter++) {
        memcpy(current, sel, kp->count * sizeof(int));

        int flips = (int)N_dim; if (flips < 1) flips = 1;
        for(int k=0; k<flips; k++) {
            int idx = rand() % kp->count; current[idx] = !current[idx];
        }

        result_t cur_res = evaluate(kp, current);
        while (cur_res.weight > kp->capacity) {
            int idx = rand() % kp->count;
            if (current[idx]) { current[idx] = 0; cur_res = evaluate(kp, current); }
        }

        run_local_search(kp, current);
        cur_res = evaluate(kp, current);

        if (cur_res.value > best.value) {
            best = cur_res; memcpy(sel, current, kp->count * sizeof(int));
            no_improve = 0; N_dim = 1.0;
        } else {
            no_improve++; N_dim += 0.5;
        }
    }
    free(current);
}

void run_size(int n_items, int capacity) {
    double sum_val_g = 0, sum_val_ls = 0, sum_val_psi = 0;
    int invalid = 0;

    for (int i = 0; i < INSTANCES; i++) {
        knapsack_t *kp = malloc(sizeof(knapsack_t));
        kp->count = n_items; kp->capacity = capacity;
        srand(1000 + i); // distinct, reproducible seed per instance
        for (int j = 0; j < kp->count; j++) {
            kp->items[j].value = rand() % 100 + 10;
            kp->items[j].weight = rand() % 30 + 5;
        }

        int *sel_g = malloc(n_items * sizeof(int));
        int *sel_ls = malloc(n_items * sizeof(int));
        int *sel_psi = malloc(n_items * sizeof(int));

        run_greedy(kp, sel_g);
        memcpy(sel_ls, sel_g, n_items * sizeof(int));
        memcpy(sel_psi, sel_g, n_items * sizeof(int));

        run_local_search(kp, sel_ls);
        run_psi(kp, sel_psi);

        result_t rg = evaluate(kp, sel_g);
        result_t rls = evaluate(kp, sel_ls);
        result_t rpsi = evaluate(kp, sel_psi);

        if (!rg.is_valid || !rls.is_valid || !rpsi.is_valid) {
            invalid++;
        } else {
            sum_val_g += rg.value;
            sum_val_ls += rls.value;
            sum_val_psi += rpsi.value;
        }

        free(sel_g); free(sel_ls); free(sel_psi); free(kp);
    }

    if (invalid > 0) {
        printf("N=%-5d  capacity=%-6d  [%d/%d instances INVALID -- skipped]\n",
               n_items, capacity, invalid, INSTANCES);
        return;
    }

    double g = sum_val_g/INSTANCES, ls = sum_val_ls/INSTANCES, psi = sum_val_psi/INSTANCES;
    printf("N=%-5d  capacity=%-6d  Greedy=%-10.1f LS=%-10.1f PSI=%-10.1f  PSI-vs-LS=%+.3f%%\n",
           n_items, capacity, g, ls, psi, (psi - ls) / ls * 100.0);
}

int main() {
    printf("Scaling test: does PSI's edge over LS grow with problem size?\n");
    printf("(capacity scaled proportionally to keep problem difficulty comparable)\n\n");

    int sizes[]     = {50,  100, 200,  500,  1000, 2000, 5000};
    int n_sizes = sizeof(sizes)/sizeof(sizes[0]);

    for (int s = 0; s < n_sizes; s++) {
        int n = sizes[s];
        int capacity = n * 5; // ~25% of max possible weight, consistent across sizes
        run_size(n, capacity);
    }

    return 0;
}
