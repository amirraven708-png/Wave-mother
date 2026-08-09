#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "wave_index.h"

#define NUM_KEYS      100000
#define PAYLOAD_SIZE  64
#define LOOKUPS       50000
#define DELETES       20000
#define PHASE_TOLERANCE 500.0f

/* ---------- Redis Sim (Hash Table) ---------- */
typedef struct kv_s { uint64_t key; char val[PAYLOAD_SIZE]; struct kv_s *next; } kv_t;
typedef struct { kv_t **buckets; size_t nb; } ht_t;

void ht_init(ht_t *h, size_t nb) { h->nb = nb; h->buckets = calloc(nb, sizeof(kv_t*)); }
void ht_insert(ht_t *h, uint64_t key, const char *val) {
    size_t b = key % h->nb; kv_t *e = malloc(sizeof(kv_t)); e->key = key;
    memcpy(e->val, val, PAYLOAD_SIZE); e->next = h->buckets[b]; h->buckets[b] = e;
}
char* ht_lookup(ht_t *h, uint64_t key) {
    size_t b = key % h->nb;
    for (kv_t *e = h->buckets[b]; e; e = e->next) if (e->key == key) return e->val;
    return NULL;
}
void ht_delete(ht_t *h, uint64_t key) {
    size_t b = key % h->nb; kv_t **pp = &h->buckets[b];
    while (*pp) { if ((*pp)->key == key) { kv_t *t = *pp; *pp = t->next; free(t); return; } pp = &(*pp)->next; }
}
void ht_free(ht_t *h) {
    for (size_t i=0;i<h->nb;i++) { kv_t *e=h->buckets[i]; while(e){ kv_t *n=e->next; free(e); e=n; } }
    free(h->buckets);
}

/* ---------- SQLite wrapper ---------- */
static sqlite3 *sq_db;
void sq_init() {
    sqlite3_open(":memory:", &sq_db);
    sqlite3_exec(sq_db, "CREATE TABLE kv (key INTEGER PRIMARY KEY, val BLOB);", 0,0,0);
    sqlite3_exec(sq_db, "PRAGMA synchronous=OFF; PRAGMA journal_mode=MEMORY;", 0,0,0);
}
void sq_insert(uint64_t key, const char *val) {
    sqlite3_stmt *st; sqlite3_prepare_v2(sq_db, "INSERT OR REPLACE INTO kv VALUES(?,?)", -1, &st, 0);
    sqlite3_bind_int64(st, 1, key); sqlite3_bind_blob(st, 2, val, PAYLOAD_SIZE, SQLITE_STATIC);
    sqlite3_step(st); sqlite3_finalize(st);
}
int sq_lookup(uint64_t key, char *val) {
    sqlite3_stmt *st; sqlite3_prepare_v2(sq_db, "SELECT val FROM kv WHERE key=?", -1, &st, 0);
    sqlite3_bind_int64(st, 1, key); int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) { memcpy(val, sqlite3_column_blob(st,0), PAYLOAD_SIZE); sqlite3_finalize(st); return 1; }
    sqlite3_finalize(st); return 0;
}
void sq_delete(uint64_t key) {
    sqlite3_stmt *st; sqlite3_prepare_v2(sq_db, "DELETE FROM kv WHERE key=?", -1, &st, 0);
    sqlite3_bind_int64(st, 1, key); sqlite3_step(st); sqlite3_finalize(st);
}
void sq_close() { sqlite3_close(sq_db); }

/* ---------- Memory reporting ---------- */
static long get_rss_kb() {
    FILE *f = fopen("/proc/self/status","r"); if(!f) return -1;
    char line[256]; long rss=-1;
    while(fgets(line,sizeof(line),f)) if(strncmp(line,"VmRSS:",6)==0) { sscanf(line+6,"%ld",&rss); break; }
    fclose(f); return rss;
}

/* ---------- Main ---------- */
int main() {
    srand(12345);
    uint64_t *keys = malloc(NUM_KEYS * sizeof(uint64_t));
    char **vals = malloc(NUM_KEYS * sizeof(char*));
    float *phases = malloc(NUM_KEYS * sizeof(float));
    for (int i=0;i<NUM_KEYS;i++) { keys[i]=((uint64_t)rand()<<32)|rand(); vals[i]=malloc(PAYLOAD_SIZE);
        for(int j=0;j<PAYLOAD_SIZE;j++) vals[i][j]=rand()&0xFF; phases[i]=(float)(rand()%10000); }

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  WAVE MEMORY vs REDIS (sim) vs SQLITE               ║\n");
    printf("║  %d keys, %d lookups, %d deletes        ║\n", NUM_KEYS, LOOKUPS, DELETES);
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    // 1. Redis sim
    long mem0 = get_rss_kb();
    ht_t ht; ht_init(&ht, 100003);
    clock_t t0=clock(); for(int i=0;i<NUM_KEYS;i++) ht_insert(&ht,keys[i],vals[i]);
    double ht_ins = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    t0=clock(); int ht_found=0; for(int i=0;i<LOOKUPS;i++) if(ht_lookup(&ht,keys[rand()%NUM_KEYS])) ht_found++;
    double ht_look = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    t0=clock(); for(int i=0;i<DELETES;i++) ht_delete(&ht,keys[rand()%NUM_KEYS]);
    double ht_del = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    long ht_mem = get_rss_kb()-mem0; ht_free(&ht);

    // 2. Wave Memory
    mem0 = get_rss_kb();
    wd_trace_t *wm_traces = malloc(NUM_KEYS * sizeof(wd_trace_t));
    for (int i=0;i<NUM_KEYS;i++) { wm_traces[i].exact_key=keys[i]; wm_traces[i].rhythm=0xABCD1234;
        wm_traces[i].phase=phases[i]; wm_traces[i].type=1; wm_traces[i].size=PAYLOAD_SIZE;
        memcpy(wm_traces[i].content, vals[i], PAYLOAD_SIZE); }
    wave_index_t wm_idx;
    t0=clock(); wave_index_build(&wm_idx, wm_traces, NUM_KEYS, 256);
    double wm_ins = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    t0=clock(); int wm_found=0; wd_trace_t out;
    for(int i=0;i<LOOKUPS;i++) if(wave_index_get_by_key(&wm_idx, keys[rand()%NUM_KEYS], &out)) wm_found++;
    double wm_look = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    wd_trace_t *phase_results = malloc(NUM_KEYS*sizeof(wd_trace_t));
    t0=clock(); int phase_count = wave_index_select_by_phase(&wm_idx, 5000.0f, PHASE_TOLERANCE, phase_results, NUM_KEYS);
    double wm_phase = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    free(phase_results);
    long wm_mem = get_rss_kb()-mem0;
    wave_index_free(&wm_idx); free(wm_traces);

    // 3. SQLite
    mem0 = get_rss_kb(); sq_init();
    t0=clock(); for(int i=0;i<NUM_KEYS;i++) sq_insert(keys[i], vals[i]);
    double sq_ins = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    t0=clock(); int sq_found=0; char sq_val[PAYLOAD_SIZE];
    for(int i=0;i<LOOKUPS;i++) if(sq_lookup(keys[rand()%NUM_KEYS], sq_val)) sq_found++;
    double sq_look = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    t0=clock(); for(int i=0;i<DELETES;i++) sq_delete(keys[rand()%NUM_KEYS]);
    double sq_del = (double)(clock()-t0)/CLOCKS_PER_SEC*1000.0;
    long sq_mem = get_rss_kb()-mem0; sq_close();

    printf("%-15s | %10s | %10s | %10s | %10s | %10s\n","Method","Insert(ms)","Lookup(ms)","Delete(ms)","Phase(ms)","Memory(kB)");
    printf("──────────────────────────────────────────────────────────────────────────────────\n");
    printf("%-15s | %10.2f | %10.2f | %10.2f | %10s | %10ld\n","Redis (sim)",ht_ins,ht_look,ht_del,"N/A",ht_mem);
    printf("%-15s | %10.2f | %10.2f | %10.2f | %10s | %10ld\n","SQLite",sq_ins,sq_look,sq_del,"N/A",sq_mem);
    printf("%-15s | %10.2f | %10.2f | %10s | %10.2f | %10ld\n","Wave Memory",wm_ins,wm_look,"N/A",wm_phase,wm_mem);
    printf("\nPhase retrieval (%d records within +/-%.0f): %.2f ms\n", phase_count, PHASE_TOLERANCE, wm_phase);

    for(int i=0;i<NUM_KEYS;i++) free(vals[i]); free(vals); free(keys); free(phases);
    return 0;
}
