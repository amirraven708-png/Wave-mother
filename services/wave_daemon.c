#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include "wave_trace.h"
#include "wave_index.h"

#define SHARED_DIR  "./data/generated"
#define SOCK_PATH   "./wave_daemon.sock"
#define MAX_ZONES   50
#define SLEEP_AFTER 100  // Sleep after 100 queries

typedef struct {
    double center_phase;
    double phase_radius;
    double stability;
    int hit_count;
    uint64_t rhythm_pattern;
} golden_zone_t;

typedef struct {
    golden_zone_t zones[MAX_ZONES];
    int zone_count;
    int total_queries;
    int queries_since_sleep;
    time_t last_sleep_time;
    double learning_rate;
    double avg_query_time;
} daemon_state_t;

static wave_index_t idx;
static wd_trace_t *traces = NULL;
static size_t trace_count = 0;
static daemon_state_t state = {0};

void load_traces(void) {
    DIR *d = opendir(SHARED_DIR);
    if (!d) { fprintf(stderr, "Warning: cannot open %s\n", SHARED_DIR); return; }
    
    struct dirent *e;
    trace_count = 0;
    while ((e = readdir(d))) if (strstr(e->d_name, ".json")) trace_count++;
    closedir(d);
    
    if (trace_count == 0) { fprintf(stderr, "No traces found\n"); return; }
    
    free(traces);
    traces = calloc(trace_count, sizeof(wd_trace_t));
    if (!traces) { fprintf(stderr, "Memory allocation failed\n"); return; }
    
    size_t idx_count = 0;
    d = opendir(SHARED_DIR);
    while ((e = readdir(d))) {
        if (!strstr(e->d_name, ".json")) continue;
        char fp[512];
        snprintf(fp, sizeof(fp), "%s/%s", SHARED_DIR, e->d_name);
        FILE *f = fopen(fp, "r");
        if (!f) continue;
        
        wd_trace_t t;
        char cont[256];
        int m = fscanf(f, "{\"rhythm\":%" SCNu64 ",\"phase\":%lf,\"type\":%u,\"size\":%u,\"content\":\"%[^\"]\"}",
                       &t.rhythm, &t.phase, &t.type, &t.size, cont);
        fclose(f);
        
        if (m == 5) {
            strcpy(t.content, cont);
            traces[idx_count] = t;
            idx_count++;
        }
    }
    closedir(d);
    
    trace_count = idx_count;
    wave_index_build(&idx, traces, trace_count, 256);
    fprintf(stderr, "📚 Loaded %zu traces into index\n", trace_count);
}

// Find or create golden zone
int find_golden_zone(uint64_t rhythm, double phase) {
    // Look for existing zone
    for (int i = 0; i < state.zone_count; i++) {
        double phase_diff = fabs(phase - state.zones[i].center_phase);
        if (state.zones[i].rhythm_pattern == rhythm && 
            phase_diff < state.zones[i].phase_radius) {
            state.zones[i].hit_count++;
            state.zones[i].stability += 0.1;
            return i;
        }
    }
    
    // Create new zone if space available
    if (state.zone_count < MAX_ZONES) {
        golden_zone_t *zone = &state.zones[state.zone_count];
        zone->center_phase = phase;
        zone->phase_radius = 500.0;  // Initial radius
        zone->stability = 0.5;
        zone->hit_count = 1;
        zone->rhythm_pattern = rhythm;
        return state.zone_count++;
    }
    
    return -1;
}

// Sleep consolidation
void daemon_sleep_consolidate(void) {
    fprintf(stderr, "\n💤 ENTERING SLEEP CONSOLIDATION (after %d queries)\n", 
            state.total_queries);
    
    // Merge overlapping zones
    int merged = 0;
    for (int i = 0; i < state.zone_count; i++) {
        for (int j = i + 1; j < state.zone_count; j++) {
            if (state.zones[i].rhythm_pattern == state.zones[j].rhythm_pattern) {
                double phase_diff = fabs(state.zones[i].center_phase - 
                                        state.zones[j].center_phase);
                double combined_radius = state.zones[i].phase_radius + 
                                        state.zones[j].phase_radius;
                
                if (phase_diff < combined_radius * 0.8) {
                    // Merge zones
                    double total_hits = state.zones[i].hit_count + state.zones[j].hit_count;
                    state.zones[i].center_phase = 
                        (state.zones[i].center_phase * state.zones[i].hit_count +
                         state.zones[j].center_phase * state.zones[j].hit_count) / total_hits;
                    state.zones[i].phase_radius = combined_radius * 0.9;
                    state.zones[i].stability = 
                        (state.zones[i].stability + state.zones[j].stability) * 0.7;
                    state.zones[i].hit_count = (int)total_hits;
                    
                    // Remove zone j
                    state.zones[j] = state.zones[--state.zone_count];
                    merged++;
                    j--;
                }
            }
        }
    }
    
    // Increase learning rate based on zone stability
    double avg_stability = 0;
    for (int i = 0; i < state.zone_count; i++) {
        avg_stability += state.zones[i].stability;
    }
    if (state.zone_count > 0) {
        avg_stability /= state.zone_count;
        state.learning_rate = 0.1 + avg_stability * 0.5;
    }
    
    state.last_sleep_time = time(NULL);
    state.queries_since_sleep = 0;
    
    fprintf(stderr, "🧠 CONSOLIDATED: %d zones (merged %d), stability: %.2f, learning rate: %.2f\n",
            state.zone_count, merged, avg_stability, state.learning_rate);
    fprintf(stderr, "☀️  WAKING UP\n\n");
}

void parse_we_url(const char *url, uint64_t *rhythm, uint64_t *pmin, uint64_t *pmax) {
    *rhythm = 0; *pmin = 0; *pmax = 99999;
    if (strncmp(url, "we://", 5) != 0) return;
    
    const char *s = url + 5;
    char hex[17] = {0};
    strncpy(hex, s, 16);
    *rhythm = strtoull(hex, NULL, 16);
    
    const char *slash = strchr(s, '/');
    if (slash) sscanf(slash+1, "%" SCNu64 "-%" SCNu64, pmin, pmax);
}

void handle_query(const char *url, int fd) {
    clock_t start = clock();
    
    uint64_t rhythm, pmin, pmax;
    parse_we_url(url, &rhythm, &pmin, &pmax);
    
    if (!rhythm) { 
        dprintf(fd, "ERROR: invalid we:// URL\n"); 
        return;
    }
    
    state.total_queries++;
    state.queries_since_sleep++;
    
    // Check if sleep needed
    if (state.queries_since_sleep >= SLEEP_AFTER) {
        daemon_sleep_consolidate();
    }
    
    // Use golden zones to optimize search
    int zone_id = find_golden_zone(rhythm, (pmin + pmax) / 2.0);
    
    wd_trace_t results[200];
    int n;
    
    if (zone_id >= 0 && state.zones[zone_id].stability > 0.7) {
        // Use zone-optimized search
        golden_zone_t *zone = &state.zones[zone_id];
        double search_radius = zone->phase_radius * state.learning_rate;
        
        n = wave_index_select(&idx, rhythm, 0xFFFFFFFFFFFFFFFFULL,
                             (uint64_t)(zone->center_phase - search_radius),
                             (uint64_t)(zone->center_phase + search_radius),
                             results, 200);
    } else {
        // Full search
        n = wave_index_select(&idx, rhythm, 0xFFFFFFFFFFFFFFFFULL,
                             pmin, pmax, results, 200);
    }
    
    double query_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    state.avg_query_time = state.avg_query_time * 0.9 + query_time * 0.1;
    
    // Send results
    dprintf(fd, "📊 Query #%d (%.2fms, zones:%d, rate:%.2f)\n", 
            state.total_queries, query_time, state.zone_count, state.learning_rate);
    
    for (int i = 0; i < n && i < 10; i++)  // Limit output
        dprintf(fd, "  Rhythm:%" PRIX64 " Phase:%.0f Content:%s\n",
                results[i].rhythm, results[i].phase, results[i].content);
    
    if (n == 0) dprintf(fd, "  (no matches)\n");
    if (n > 10) dprintf(fd, "  ... and %d more results\n", n - 10);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     WAVE MEMORY DAEMON WITH GOLDEN ZONES               ║\n");
    printf("║     Sleep/Wake Learning Enabled                        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    state.learning_rate = 0.5;
    state.last_sleep_time = time(NULL);
    
    load_traces();
    
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    
    unlink(SOCK_PATH);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, SOCK_PATH);
    
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); return 1; }
    
    fprintf(stderr, "🌊 Wave Daemon active on %s\n", SOCK_PATH);
    fprintf(stderr, "💡 Features: Golden Zones + Sleep/Wake Learning\n\n");
    fflush(stderr);
    
    while (1) {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) { perror("accept"); continue; }
        
        char buf[256] = {0};
        read(cli, buf, sizeof(buf)-1);
        handle_query(buf, cli);
        close(cli);
    }
    
    close(srv);
    wave_index_free(&idx);
    free(traces);
    return 0;
}
