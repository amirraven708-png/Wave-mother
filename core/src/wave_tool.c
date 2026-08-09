#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wave_trace.h"
#include "wave_index.h"

void usage(const char *prog) {
    fprintf(stderr,
        "Wave Mother CLI Tool\n\n"
        "  %s build <csv_file>        - load data and enter interactive mode\n"
        "  %s get <key>               - exact key lookup\n"
        "  %s query <rhythm> <mask> <pmin> <pmax>\n"
        "  %s phase <target> <tolerance>\n"
        "  %s mesh <P>                - run mesh benchmark\n"
        "  %s benchmark               - run full comparative benchmark\n"
        "  %s interactive             - enter interactive mode after loading data\n"
        "  %s help\n\n"
        "Interactive mode commands (after 'build'):\n"
        "  get <key>\n"
        "  query <rhythm_hex> <mask_hex> <pmin> <pmax>\n"
        "  phase <target> <tolerance>\n"
        "  stats\n"
        "  exit\n", prog, prog, prog, prog, prog, prog, prog);
}

static wave_index_t idx;
static wd_trace_t *traces = NULL;
static size_t trace_count = 0;

static int load_csv(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long key_ul, rhythm_ul;
        float phase;
        char payload[256];
        
        int matched = sscanf(line, "%lu,0x%lx,%f,%255[^\n]", &key_ul, &rhythm_ul, &phase, payload);
        if (matched >= 3) {
            traces = realloc(traces, (trace_count+1)*sizeof(wd_trace_t));
            wd_trace_t *t = &traces[trace_count];
            t->exact_key = (uint64_t)key_ul;
            t->rhythm = (uint64_t)rhythm_ul;
            t->phase = (double)phase;
            t->type = 1;
            if (matched == 4) {
                t->size = strlen(payload);
                memcpy(t->content, payload, t->size);
            } else {
                t->size = 0;
                t->content[0] = '\0';
            }
            trace_count++;
        }
    }
    fclose(f);
    
    if (trace_count > 0) {
        wave_index_build(&idx, traces, trace_count, 256);
        printf("Loaded %zu traces from %s\n", trace_count, path);
    } else {
        printf("No valid records found in %s\n", path);
    }
    return 0;
}

static void cmd_get(const char *arg) {
    uint64_t key = strtoull(arg, NULL, 0);
    wd_trace_t out;
    if (wave_index_get_by_key(&idx, key, &out)) {
        printf("Found: key=%lu rhythm=0x%lX phase=%.2f content=%s\n",
               out.exact_key, out.rhythm, out.phase, out.content);
    } else printf("Not found.\n");
}

static void cmd_query(const char *r, const char *m, const char *pmin, const char *pmax) {
    uint64_t rhythm = strtoull(r, NULL, 16);
    uint64_t mask   = strtoull(m, NULL, 16);
    uint64_t mn     = strtoull(pmin, NULL, 10);
    uint64_t mx     = strtoull(pmax, NULL, 10);
    wd_trace_t res[100];
    int n = wave_index_select(&idx, rhythm, mask, mn, mx, res, 100);
    printf("Found %d traces:\n", n);
    for (int i = 0; i < n && i < 10; i++)
        printf("  rhythm=0x%lX phase=%.2f content=%s\n", res[i].rhythm, res[i].phase, res[i].content);
}

static void cmd_phase(const char *target, const char *tol) {
    float t = atof(target), l = atof(tol);
    wd_trace_t res[100];
    int n = wave_index_select_by_phase(&idx, t, l, res, 100);
    printf("Found %d traces within phase [%.2f - %.2f]:\n", n, t-l, t+l);
    for (int i = 0; i < n && i < 5; i++)
        printf("  key=%lu phase=%.2f content=%s\n", res[i].exact_key, res[i].phase, res[i].content);
}

static void interactive_loop() {
    char line[512];
    printf("\nWave Mother interactive shell. Type 'help' for commands, 'exit' to quit.\n\n");
    while (1) {
        printf("wave> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        
        char cmd[64], a1[128], a2[128], a3[128], a4[128];
        int n = sscanf(line, "%63s %127s %127s %127s %127s", cmd, a1, a2, a3, a4);
        
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) break;
        else if (strcmp(cmd, "help") == 0) {
            printf("Commands: get <key>, query <rhythm_hex> <mask_hex> <pmin> <pmax>, phase <target> <tolerance>, stats, exit\n");
        }
        else if (strcmp(cmd, "stats") == 0) {
            printf("Loaded traces: %zu\n", trace_count);
            printf("Index buckets (rhythm): %zu\n", idx.num_buckets_rhythm);
            printf("Index buckets (key): %zu\n", idx.num_buckets_key);
        }
        else if (strcmp(cmd, "get") == 0 && n >= 2) cmd_get(a1);
        else if (strcmp(cmd, "query") == 0 && n >= 5) cmd_query(a1, a2, a3, a4);
        else if (strcmp(cmd, "phase") == 0 && n >= 3) cmd_phase(a1, a2);
        else printf("Unknown command or missing arguments. Type 'help'.\n");
    }
    printf("Goodbye.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0) {
        usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "build") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        load_csv(argv[2]);
        interactive_loop();
        return 0;
    }

    if (strcmp(cmd, "interactive") == 0) {
        if (!traces) {
            fprintf(stderr, "No data loaded. Run '%s build <csv>' first.\n", argv[0]);
            return 1;
        }
        interactive_loop();
        return 0;
    }

    /* one‑shot commands (load data first) */
    if (strcmp(cmd, "mesh") == 0) {
        int P = argc > 2 ? atoi(argv[2]) : 1000;
        printf("Mesh benchmark P=%d...\n", P);
        char buf[128];
        snprintf(buf, sizeof(buf), "./core/bin/benchmark_mesh %d", P);
        system(buf);
        return 0;
    }

    if (strcmp(cmd, "benchmark") == 0) {
        printf("Running final comparative benchmark...\n");
        system("./core/bin/benchmark_final_v2");
        return 0;
    }

    /* for get/query/phase, load data first */
    if (!traces) {
        fprintf(stderr, "No data loaded. Run '%s build <csv>' first.\n", argv[0]);
        return 1;
    }

    if (strcmp(cmd, "get") == 0 && argc >= 3) cmd_get(argv[2]);
    else if (strcmp(cmd, "query") == 0 && argc >= 6) cmd_query(argv[2], argv[3], argv[4], argv[5]);
    else if (strcmp(cmd, "phase") == 0 && argc >= 4) cmd_phase(argv[2], argv[3]);
    else usage(argv[0]);

    // cleanup
    wave_index_free(&idx);
    free(traces);
    return 0;
}
