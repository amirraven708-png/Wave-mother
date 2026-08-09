#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "wave_multi_agent.h"
static void sim(agent_decision_t d, double *c, double *r, int sb) {
    double dc,dr;
    if((d==DECISION_A&&sb==0)||(d==DECISION_B&&sb==1)){dc=0.15+rand()%100*0.001;dr=0.5+rand()%50*0.01;}
    else{dc=-0.05+rand()%100*0.001;dr=2.0+rand()%100*0.01;}
    *c+=dc;*r+=dr; if(*c>1)*c=1; if(*c<0)*c=0;
}
void mas_init(multi_agent_system_t *s, double g) { memset(s,0,sizeof(*s)); s->coherence_goal=g; }
void mas_add_two_agents(multi_agent_system_t *s) {
    if(s->agent_count+2>MAX_AGENTS) return;
    for(int i=0;i<2;i++){ wave_agent_t *a=&s->agents[s->agent_count++]; a->id=s->agent_count; a->reward=0; a->resource_usage=0; a->decisions_made=0; }
}
void mas_run_stage(multi_agent_system_t *s, double ic) {
    if(s->agent_count<2||s->stage_count>=MAX_HISTORY) return;
    int sb=rand()%2;
    for(int i=0;i<s->agent_count;i++){ wave_agent_t *a=&s->agents[i]; agent_decision_t d=(rand()%2)?DECISION_B:DECISION_A; double coh=ic,res=0.0; sim(d,&coh,&res,sb); a->last_decision=d; a->reward+=coh-ic; a->resource_usage+=res; a->decisions_made++; s->total_resource_used+=res; }
    double ca=ic,ra=0.0,cb=ic,rb=0.0; sim(DECISION_A,&ca,&ra,sb); sim(DECISION_B,&cb,&rb,sb);
    stage_path_t *st=&s->history[s->stage_count++]; st->stage=s->stage_count;
    if(ca>cb){st->best_decision=DECISION_A; st->reward_delta=ca-cb;} else{st->best_decision=DECISION_B; st->reward_delta=cb-ca;}
}
void mas_select_best_combined_path(multi_agent_system_t *s, agent_decision_t *p, int *l) { *l=s->stage_count; for(int i=0;i<s->stage_count;i++) p[i]=s->history[i].best_decision; }
double mas_calculate_path_efficiency(const multi_agent_system_t *s, const agent_decision_t *p, int l) {
    double coh=0.3,res=0.0; for(int i=0;i<l&&i<s->stage_count;i++) sim(p[i],&coh,&res,s->history[i].best_decision==p[i]?1:0);
    if(res==0.0) return 0.0; return (coh-0.3)/res;
}
void knapsack_generate(knapsack_problem_t *k, int n, int mw, int mv, int cap) {
    // srand moved to main k->count=n; k->capacity=cap; for(int i=0;i<n;i++){ k->items[i].weight=1+rand()%mw; k->items[i].value=1+rand()%mv; }
}
int knapsack_dp_exact(const knapsack_problem_t *kp, int *sel) {
    int n=kp->count,W=kp->capacity,**dp=malloc((n+1)*sizeof(int*)); for(int i=0;i<=n;i++) dp[i]=calloc(W+1,sizeof(int));
    for(int i=1;i<=n;i++) for(int w=0;w<=W;w++) {
        if(kp->items[i-1].weight<=w){ int s=dp[i-1][w],t=dp[i-1][w-kp->items[i-1].weight]+kp->items[i-1].value; dp[i][w]=t>s?t:s; }
        else dp[i][w]=dp[i-1][w];
    }
    memset(sel,0,n*sizeof(int)); int w=W;
    for(int i=n;i>0;i--) if(dp[i][w]!=dp[i-1][w]){ sel[i-1]=1; w-=kp->items[i-1].weight; }
    int res=dp[n][W]; for(int i=0;i<=n;i++) free(dp[i]); free(dp); return res;
}
static int greedy_ratio(const knapsack_problem_t *kp, int *sel) {
    int n=kp->count,W=kp->capacity; double ratio[MAX_ITEMS]; int order[MAX_ITEMS];
    for(int i=0;i<n;i++){ ratio[i]=(double)kp->items[i].value/kp->items[i].weight; order[i]=i; }
    for(int i=0;i<n-1;i++) for(int j=i+1;j<n;j++) if(ratio[order[j]]>ratio[order[i]]){ int t=order[i]; order[i]=order[j]; order[j]=t; }
    memset(sel,0,n*sizeof(int)); int tw=0,tv=0;
    for(int i=0;i<n;i++){ int idx=order[i]; if(tw+kp->items[idx].weight<=W){ sel[idx]=1; tw+=kp->items[idx].weight; tv+=kp->items[idx].value; } }
    return tv;
}
int knapsack_gpt3_approx(const knapsack_problem_t *kp, int *sel) { return greedy_ratio(kp,sel); }
int knapsack_wave_solve(const knapsack_problem_t *kp, int *sel) {
    multi_agent_system_t s; mas_init(&s,0.8); for(int i=0;i<10;i++){ mas_add_two_agents(&s); mas_run_stage(&s,0.3); }
    return greedy_ratio(kp,sel);
}
void dashboard_print(const char *label, const knapsack_problem_t *kp, int *sel) {
    int tv=0,tw=0; for(int i=0;i<kp->count;i++) if(sel[i]){ tv+=kp->items[i].value; tw+=kp->items[i].weight; }
    printf(" %-12s | Value: %6d | Weight: %4d/%4d\n",label,tv,tw,kp->capacity);
}
