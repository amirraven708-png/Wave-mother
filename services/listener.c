#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <inttypes.h>
#include "wave_trace.h"
#include "wave_index.h"

#define SHARED_DIR  "./data/generated"
#define SOCK_PATH   "./wave_listener.sock"

static wave_index_t idx;
static wd_trace_t *traces = NULL;
static size_t trace_count = 0;

void load_traces(void) {
    DIR *d = opendir(SHARED_DIR);
    if (!d) { fprintf(stderr, "Warning: cannot open %s\n", SHARED_DIR); return; }
    
    struct dirent *e;
    while ((e = readdir(d))) if (strstr(e->d_name, ".json")) trace_count++;
    closedir(d);
    
    if (trace_count == 0) { fprintf(stderr, "No traces found in %s\n", SHARED_DIR); return; }
    
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
    
    if (idx_count == 0) { free(traces); traces = NULL; trace_count = 0; return; }
    trace_count = idx_count;
    
    wave_index_build(&idx, traces, trace_count, 256);
    fprintf(stderr, "Listener: loaded %zu traces into index.\n", trace_count);
    fflush(stderr);
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
    uint64_t rhythm, pmin, pmax;
    parse_we_url(url, &rhythm, &pmin, &pmax);
    
    if (!rhythm) { dprintf(fd, "ERROR: invalid we:// URL\n"); return; }
    
    wd_trace_t results[100];
    int n = wave_index_select(&idx, rhythm, 0xFFFFFFFFFFFFFFFFULL, pmin, pmax, results, 100);
    
    for (int i = 0; i < n; i++)
        dprintf(fd, "Rhythm:%" PRIX64 " Phase:%.0f Content:%s\n",
                results[i].rhythm, results[i].phase, results[i].content);
    if (n == 0) dprintf(fd, "(no matches)\n");
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    load_traces();
    
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    
    unlink(SOCK_PATH);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, SOCK_PATH);
    
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 5) < 0) { perror("listen"); return 1; }
    
    fprintf(stderr, "Listener active on %s\n", SOCK_PATH);
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
