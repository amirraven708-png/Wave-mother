#!/bin/bash
set -e

echo "============================================"
echo "🌊 Wave Memory - Complete Setup"
echo "============================================"

# 1. Create directories
mkdir -p include/wd src services data/generated

# 2. wave_trace.h
cat > include/wd/wave_trace.h << 'HDR'
#ifndef WD_WAVE_TRACE_H
#define WD_WAVE_TRACE_H
#include <stdint.h>
#include <stddef.h>
#define WD_MAX_CONTENT_SIZE 256
typedef struct {
    uint64_t rhythm;
    double phase;
    uint32_t type;
    uint32_t size;
    char content[WD_MAX_CONTENT_SIZE];
} wd_trace_t;
#endif
HDR

ln -sf wd/wave_trace.h include/wave_trace.h

# 3. wave_index.h
cat > include/wave_index.h << 'HDR'
#ifndef WAVE_INDEX_H
#define WAVE_INDEX_H
#include <stdint.h>
#include <stddef.h>
#include "wave_trace.h"
typedef struct {
    size_t count;
    size_t capacity;
    size_t *trace_indices;
} rhythm_bucket_t;
typedef struct {
    size_t num_buckets;
    size_t num_traces;
    rhythm_bucket_t *buckets;
    const wd_trace_t *traces;
} wave_index_t;
void wave_index_build(wave_index_t *idx, const wd_trace_t *traces, size_t count, int num_buckets);
int wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t rhythm_mask,
                      uint64_t phase_min, uint64_t phase_max,
                      wd_trace_t *results, int max_results);
void wave_index_free(wave_index_t *idx);
#endif
HDR

# 4. wave_index.c
cat > src/wave_index.c << 'SRC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "wave_index.h"

static uint64_t hash_rhythm(uint64_t rhythm, size_t num_buckets) {
    uint64_t h = rhythm ^ (rhythm >> 32);
    h ^= h >> 16; h *= 0x85EBCA6B; h ^= h >> 13; h *= 0xC2B2AE35; h ^= h >> 16;
    return h % num_buckets;
}

void wave_index_build(wave_index_t *idx, const wd_trace_t *traces, size_t count, int num_buckets) {
    idx->num_buckets = (size_t)num_buckets;
    idx->num_traces = count;
    idx->traces = traces;
    idx->buckets = calloc(num_buckets, sizeof(rhythm_bucket_t));
    if (!idx->buckets) return;
    
    for (size_t i = 0; i < count; i++) {
        uint64_t h = hash_rhythm(traces[i].rhythm, idx->num_buckets);
        idx->buckets[h].count++;
    }
    
    for (size_t i = 0; i < idx->num_buckets; i++) {
        if (idx->buckets[i].count > 0) {
            idx->buckets[i].capacity = idx->buckets[i].count;
            idx->buckets[i].trace_indices = malloc(idx->buckets[i].count * sizeof(size_t));
            idx->buckets[i].count = 0;
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        uint64_t h = hash_rhythm(traces[i].rhythm, idx->num_buckets);
        idx->buckets[h].trace_indices[idx->buckets[h].count++] = i;
    }
}

int wave_index_select(const wave_index_t *idx, uint64_t rhythm, uint64_t rhythm_mask,
                      uint64_t phase_min, uint64_t phase_max,
                      wd_trace_t *results, int max_results) {
    if (!idx || !idx->buckets || !results) return 0;
    uint64_t h = hash_rhythm(rhythm, idx->num_buckets);
    rhythm_bucket_t *bucket = &idx->buckets[h];
    int found = 0;
    
    for (size_t i = 0; i < bucket->count && found < max_results; i++) {
        size_t ti = bucket->trace_indices[i];
        const wd_trace_t *t = &idx->traces[ti];
        if ((t->rhythm & rhythm_mask) != (rhythm & rhythm_mask)) continue;
        if (t->phase < (double)phase_min || t->phase > (double)phase_max) continue;
        results[found++] = *t;
    }
    return found;
}

void wave_index_free(wave_index_t *idx) {
    if (!idx || !idx->buckets) return;
    for (size_t i = 0; i < idx->num_buckets; i++) free(idx->buckets[i].trace_indices);
    free(idx->buckets);
    idx->buckets = NULL;
    idx->num_buckets = 0;
    idx->num_traces = 0;
}
SRC

# 5. producer.c
cat > services/producer.c << 'SRC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include "wave_trace.h"

#define SHARED_DIR "./data/generated"

int main(void) {
    system("mkdir -p " SHARED_DIR);
    srand(time(NULL));
    
    for (int i = 0; i < 100; i++) {
        wd_trace_t t;
        t.rhythm = 0xABCD1234;
        t.phase = (double)(i * 100);
        t.type = 1;
        t.size = 12;
        strcpy(t.content, "Hello Wave!");
        
        char fname[256];
        snprintf(fname, sizeof(fname), "%s/trace_%04d.json", SHARED_DIR, i);
        FILE *f = fopen(fname, "w");
        if (f) {
            fprintf(f, "{\"rhythm\":%" PRIu64 ",\"phase\":%f,\"type\":%u,\"size\":%u,\"content\":\"%s\"}\n",
                    t.rhythm, t.phase, t.type, t.size, t.content);
            fclose(f);
            printf("Produced %s (phase=%f)\n", fname, t.phase);
        }
    }
    return 0;
}
SRC

# 6. listener.c
cat > services/listener.c << 'SRC'
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
SRC

# 7. terminal.c
cat > services/terminal.c << 'SRC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "./wave_listener.sock"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s we://<rhythm>[/<phase>]\n", argv[0]); return 1; }
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, SOCK_PATH);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect to listener");
        return 1;
    }
    
    write(sock, argv[1], strlen(argv[1]) + 1);
    
    char buf[4096];
    ssize_t n = read(sock, buf, sizeof(buf)-1);
    if (n > 0) { buf[n] = '\0'; printf("%s", buf); }
    
    close(sock);
    return 0;
}
SRC

# 8. Compile all
echo "Compiling..."
gcc -Wall -O2 -Iinclude -o services/producer services/producer.c
gcc -Wall -O2 -Iinclude -o services/listener services/listener.c src/wave_index.c
gcc -Wall -O2 -Iinclude -o services/terminal services/terminal.c

# 9. Run producer
echo "Generating traces..."
./services/producer

# 10. Kill old listener and start new one
pkill -f services/listener 2>/dev/null || true
rm -f ./wave_listener.sock
echo "Starting listener..."
nohup ./services/listener > listener.log 2>&1 &
sleep 2

# 11. Test terminal
echo "Testing terminal..."
./services/terminal we://ABCD1234

echo ""
echo "============================================"
echo "✅ Setup complete. To test again later:"
echo "  ./services/listener &"
echo "  ./services/terminal we://ABCD1234"
echo "============================================"
