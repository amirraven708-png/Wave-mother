#ifndef WAVE_MULTI_AGENT_H
#define WAVE_MULTI_AGENT_H
#include <stdint.h>
#define MAX_AGENTS         64
#define MAX_ITEMS          200
#define MAX_HISTORY        256
#define MAX_COMBINED_PATH  128
typedef enum { DECISION_A=0, DECISION_B=1 } agent_decision_t;
typedef struct { uint64_t id; double reward, resource_usage; agent_decision_t last_decision; int decisions_made; } wave_agent_t;
typedef struct { uint64_t stage; agent_decision_t best_decision; double reward_delta; } stage_path_t;
typedef struct { wave_agent_t agents[MAX_AGENTS]; int agent_count; stage_path_t history[MAX_HISTORY]; int stage_count; double total_resource_used, coherence_goal; } multi_agent_system_t;
typedef struct { int weight, value; } item_t;
typedef struct { item_t items[MAX_ITEMS]; int count, capacity; } knapsack_problem_t;
void mas_init(multi_agent_system_t *s, double g);
void mas_add_two_agents(multi_agent_system_t *s);
void mas_run_stage(multi_agent_system_t *s, double ic);
void mas_select_best_combined_path(multi_agent_system_t *s, agent_decision_t *p, int *l);
double mas_calculate_path_efficiency(const multi_agent_system_t *s, const agent_decision_t *p, int l);
void knapsack_generate(knapsack_problem_t *k, int n, int mw, int mv, int cap);
int  knapsack_dp_exact(const knapsack_problem_t *k, int *sel);
int  knapsack_gpt3_approx(const knapsack_problem_t *k, int *sel);
int  knapsack_wave_solve(const knapsack_problem_t *k, int *sel);
void dashboard_print(const char *label, const knapsack_problem_t *k, int *sel);
#endif
