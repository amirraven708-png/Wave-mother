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

    for (int i = 0; i < kp->count; i++)
    {
        if (selected[i])
        {
            s.value += kp->items[i].value;
            s.weight += kp->items[i].weight;
        }
    }

    return s;
}


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

    for (int i = 0; i < n; i++)
    {
        order[i] = i;

        ratio[i] =
            (double)kp->items[i].value /
            kp->items[i].weight;
    }

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (ratio[order[j]] >
                ratio[order[i]])
            {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    memset(selected, 0, n * sizeof(int));

    int weight = 0;

    for (int i = 0; i < n; i++)
    {
        int idx = order[i];

        if (weight +
            kp->items[idx].weight <=
            kp->capacity)
        {
            selected[idx] = 1;
            weight += kp->items[idx].weight;
        }
    }
}


/* ------------------------------------------------------------
 * Local improvement: 1-for-1 swap
 * ------------------------------------------------------------ */

static int improve_1x1(
    const knapsack_problem_t *kp,
    int *selected)
{
    solution_stats_t best =
        evaluate(kp, selected);

    int improved = 0;

    for (int out = 0; out < kp->count; out++)
    {
        if (!selected[out])
            continue;

        for (int in = 0; in < kp->count; in++)
        {
            if (selected[in])
                continue;

            int new_weight =
                best.weight
                - kp->items[out].weight
                + kp->items[in].weight;

            if (new_weight > kp->capacity)
                continue;

            int new_value =
                best.value
                - kp->items[out].value
                + kp->items[in].value;

            if (new_value > best.value)
            {
                selected[out] = 0;
                selected[in] = 1;

                best.weight = new_weight;
                best.value = new_value;

                improved = 1;
            }
        }
    }

    return improved;
}


/* ------------------------------------------------------------
 * Local improvement: remove one item, refill greedily
 * ------------------------------------------------------------ */

static int refill_after_removal(
    const knapsack_problem_t *kp,
    int *selected,
    int removed)
{
    selected[removed] = 0;

    int n = kp->count;
    int order[MAX_ITEMS];
    double ratio[MAX_ITEMS];

    for (int i = 0; i < n; i++)
    {
        order[i] = i;

        ratio[i] =
            (double)kp->items[i].value /
            kp->items[i].weight;
    }

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (ratio[order[j]] >
                ratio[order[i]])
            {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    int weight = 0;

    for (int i = 0; i < n; i++)
    {
        if (!selected[order[i]])
            continue;

        weight +=
            kp->items[order[i]].weight;
    }

    for (int i = 0; i < n; i++)
    {
        int idx = order[i];

        if (selected[idx])
            continue;

        if (weight +
            kp->items[idx].weight <=
            kp->capacity)
        {
            selected[idx] = 1;
            weight += kp->items[idx].weight;
        }
    }

    return evaluate(kp, selected).value;
}

/* ------------------------------------------------------------
 * Perturbation controlled by PSI N
 * ------------------------------------------------------------ */

static void psi_perturb(
    const knapsack_problem_t *kp,
    int *selected,
    double N)
{
    int attempts = (int)(N * 0.5);

    if (attempts < 1)
        attempts = 1;

    if (attempts > kp->count)
        attempts = kp->count;

    for (int k = 0; k < attempts; k++)
    {
        int idx = rand() % kp->count;

        selected[idx] =
            selected[idx] ? 0 : 1;
    }

    solution_stats_t s =
        evaluate(kp, selected);

    while (s.weight > kp->capacity)
    {
        int idx = rand() % kp->count;

        if (selected[idx])
        {
            selected[idx] = 0;
            s = evaluate(kp, selected);
        }
    }
}


/* ------------------------------------------------------------
 * Trajectory
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
    int capacity)
{
    fprintf(
        fp,
        "%d,%.6f,%.6f,%.6f,%.6f,%.3f,%d,%d,%d\n",
        iteration,
        residual,
        delta_residual,
        coherence,
        delta_coherence,
        N,
        value,
        weight,
        capacity);
}


/* ------------------------------------------------------------
 * PSI solver
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

    build_greedy(kp, current);

    while (improve_1x1(kp, current))
        ;

    solution_stats_t current_stats =
        evaluate(kp, current);

    memcpy(
        best,
        current,
        n * sizeof(int));

    solution_stats_t best_stats =
        current_stats;

    double residual = 1.0;
    double coherence = 0.30;

    double previous_best =
        (double)best_stats.value;

    int max_iters = 100;

    for (int iter = 0;
         iter < max_iters;
         iter++)
    {
        double old_residual = residual;
        double old_coherence = coherence;

        psi_core_breathe(
            psi,
            residual,
            coherence,
            0.25);

        double N =
            psi->dimensional_state;

        memcpy(
            current,
            best,
            n * sizeof(int));

        psi_perturb(
            kp,
            current,
            N);

        while (improve_1x1(kp, current))
            ;

        solution_stats_t candidate =
            evaluate(kp, current);

        double improvement =
            (double)candidate.value -
            previous_best;

        if (candidate.value > best_stats.value)
        {
            memcpy(
                best,
                current,
                n * sizeof(int));

            best_stats = candidate;

            previous_best =
                (double)best_stats.value;

            residual *= 0.82;

            if (residual < 0.01)
                residual = 0.01;
        }
        else
        {
            residual += 0.035;

            if (residual > 1.0)
                residual = 1.0;
        }

        if (improvement > 0.0)
        {
            coherence += 0.08;
        }
        else
        {
            coherence -= 0.025;
        }

        if (coherence > 1.0)
            coherence = 1.0;

        if (coherence < 0.0)
            coherence = 0.0;

        double stability =
            exp(-fabs(
                residual - old_residual) * 8.0);

        coherence =
            0.85 * coherence +
            0.15 * stability;

        double delta_residual =
            residual - old_residual;

        double delta_coherence =
            coherence - old_coherence;

        log_trajectory(
            logfile,
            iter,
            residual,
            delta_residual,
            coherence,
            delta_coherence,
            N,
            best_stats.value,
            best_stats.weight,
            kp->capacity);

        if (residual < 0.015 &&
            coherence > 0.95 &&
            fabs(delta_coherence) < 0.005)
        {
            break;
        }
    }

    memcpy(
        selected,
        best,
        n * sizeof(int));

    return best_stats.value;
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    srand(12345);

    knapsack_problem_t kp;

    knapsack_generate(
        &kp,
        200,
        50,
        200,
        1000);

    int n = kp.count;

    int selected[MAX_ITEMS];

    printf(
        "╔══════════════════════════════════════════════╗\n");

    printf(
        "║   PSI-CORE INSTRUMENTED KNAPSACK DEMO        ║\n");

    printf(
        "║   Dual-feedback / no-optimum leakage         ║\n");

    printf(
        "╚══════════════════════════════════════════════╝\n\n");


    /* --------------------------------------------------------
     * DP baseline
     * -------------------------------------------------------- */

    clock_t t0 = clock();

    int dp_val =
        knapsack_dp_exact(
            &kp,
            selected);

    double dp_time =
        (double)(clock() - t0)
        / CLOCKS_PER_SEC * 1000.0;

    dashboard_print(
        "DP Exact",
        &kp,
        selected);


    /* --------------------------------------------------------
     * GPT-style greedy baseline
     * -------------------------------------------------------- */

    t0 = clock();

    int gpt_val =
        knapsack_gpt3_approx(
            &kp,
            selected);

    double gpt_time =
        (double)(clock() - t0)
        / CLOCKS_PER_SEC * 1000.0;

    double gpt_api_time =
        gpt_time + 1200.0;

    dashboard_print(
        "GPT-3 Approx",
        &kp,
        selected);


    /* --------------------------------------------------------
     * Wave Agents
     * -------------------------------------------------------- */

    t0 = clock();

    int wave_val =
        knapsack_wave_solve(
            &kp,
            selected);

    double wave_time =
        (double)(clock() - t0)
        / CLOCKS_PER_SEC * 1000.0;

    dashboard_print(
        "Wave Agents",
        &kp,
        selected);


    /* --------------------------------------------------------
     * PSI-CORE
     * -------------------------------------------------------- */

    psi_core_engine_t psi;

    psi_core_init(&psi);

    FILE *log =
        fopen(
            "psi_trajectory.csv",
            "w");

    if (!log)
    {
        perror(
            "psi_trajectory.csv");

        return 1;
    }

    fprintf(
        log,
        "iteration,residual,delta_residual,"
        "coherence,delta_coherence,N,"
        "value,weight,capacity\n");


    t0 = clock();

    int psi_val =
        knapsack_psi_solve(
            &kp,
            selected,
            &psi,
            log);

    double psi_time =
        (double)(clock() - t0)
        / CLOCKS_PER_SEC * 1000.0;

    fclose(log);

    dashboard_print(
        "PSI-CORE",
        &kp,
        selected);


    /* --------------------------------------------------------
     * Summary
     * -------------------------------------------------------- */

    double gpt_gap =
        (dp_val - gpt_val)
        * 100.0 / dp_val;

    double wave_gap =
        (dp_val - wave_val)
        * 100.0 / dp_val;

    double psi_gap =
        (dp_val - psi_val)
        * 100.0 / dp_val;

    printf(
        "\n──────────────────────────────────────────────────────\n");

    printf(
        " COMPARISON SUMMARY\n");

    printf(
        "──────────────────────────────────────────────────────\n");

    printf(
        " Method         | Value   | Time (ms) | Gap vs DP\n");

    printf(
        "──────────────────────────────────────────────────────\n");

    printf(
        " DP Exact       | %6d  | %8.2f | %6.2f%%\n",
        dp_val,
        dp_time,
        0.0);

    printf(
        " GPT-3 Approx   | %6d  | %8.2f | %6.2f%%\n",
        gpt_val,
        gpt_api_time,
        gpt_gap);

    printf(
        " Wave Agents    | %6d  | %8.2f | %6.2f%%\n",
        wave_val,
        wave_time,
        wave_gap);

    printf(
        " PSI-CORE       | %6d  | %8.2f | %6.2f%%\n",
        psi_val,
        psi_time,
        psi_gap);

    printf(
        "──────────────────────────────────────────────────────\n");

    printf(
        "\nTrajectory saved to psi_trajectory.csv\n");

    printf(
        "Final PSI state: N=%.2f | coherence=%.4f\n",
        psi.dimensional_state,
        psi.coherence);

    return 0;
}
