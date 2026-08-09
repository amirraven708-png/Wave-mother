#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <inttypes.h>
#include "wave_trace.h"
#include "wave_index.h"
#include "need_signal.h"

#define SHARED_DIR  "./data/generated"
#define SOCK_PATH   "./wave_listener.sock"
#define MAX_TRACES  200          /* ظرفیت اسمی حافظه */
#define PRODUCE_PER_CYCLE 10     /* تعداد Trace تولیدی در هر چرخه */

static wave_index_t idx;
static wd_trace_t *traces = NULL;
static size_t trace_count = 0;
static int server_fd = -1;

/* ── بارگذاری اولیه Traceها از پوشه ── */
void load_initial_traces(void) {
    DIR *d = opendir(SHARED_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strstr(e->d_name, ".json")) trace_count++;
    }
    closedir(d);

    traces = calloc(trace_count, sizeof(wd_trace_t));
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
            traces[idx_count++] = t;
        }
    }
    closedir(d);
    trace_count = idx_count;
    wave_index_build(&idx, traces, trace_count, 256);
    printf("Initial load: %zu traces indexed.\n", trace_count);
}

/* ── افزودن یک Trace جدید در زمان اجرا ── */
void inject_trace(uint64_t rhythm, double phase, const char *content) {
    traces = realloc(traces, (trace_count + 1) * sizeof(wd_trace_t));
    wd_trace_t *t = &traces[trace_count];
    t->rhythm = rhythm;
    t->phase = phase;
    t->type = 1;
    t->size = strlen(content);
    strcpy(t->content, content);
    trace_count++;
    /* بازسازی ایندکس (در نسخه نهایی باید افزایشی باشد) */
    wave_index_build(&idx, traces, trace_count, 256);
}

/* ── پاسخ به کوئری we:// (همانند listener) ── */
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
        dprintf(fd, "Rhythm:%" PRIX64 " Phase:%.0f Content:%s\n", results[i].rhythm, results[i].phase, results[i].content);
    if (!n) dprintf(fd, "(no matches)\n");
}

/* ── حلقه اصلی الاستیک ── */
int main(void) {
    signal(SIGPIPE, SIG_IGN);
    system("mkdir -p " SHARED_DIR);
    load_initial_traces();

    /* راه‌اندازی سوکت listener */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(SOCK_PATH);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, SOCK_PATH);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    printf("Balancer active on %s\n", SOCK_PATH);

    int cycle = 0;
    need_signal_t *sig = NULL;

    while (1) {
        /* ── ۱. تولید موج (Wave Generation) ── */
        for (int i = 0; i < PRODUCE_PER_CYCLE; i++) {
            char content[64];
            snprintf(content, sizeof(content), "data_%d_%d", cycle, i);
            inject_trace(0xABCD1234, (double)(cycle * 100 + i), content);
        }

        /* ── ۲. بررسی تعادل (Elastic Check) ── */
        uint64_t demand = 500;               /* تقاضای فرضی شبکه */
        if (trace_count < demand && !need_signal_is_active(sig)) {
            sig = need_signal_trigger(0xABCD, demand, trace_count);
        } else if (sig && trace_count >= demand) {
            need_signal_supply(sig, trace_count - (demand - sig->deficit));
            need_signal_free(sig);
            sig = NULL;
        }

        /* ── ۳. سرویس کوئری‌های ورودی (Non‑blocking) ── */
        fd_set rfds;
        struct timeval tv = {0, 100000};  /* 100 ms timeout */
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        if (select(server_fd + 1, &rfds, NULL, NULL, &tv) > 0) {
            int cli = accept(server_fd, NULL, NULL);
            if (cli >= 0) {
                char buf[256] = {0};
                read(cli, buf, sizeof(buf)-1);
                handle_query(buf, cli);
                close(cli);
            }
        }

        printf("Cycle %d | traces=%zu | signal=%s\n", cycle, trace_count,
               need_signal_is_active(sig) ? "ACTIVE" : "OK");
        cycle++;
        usleep(500000);  /* 500 ms */
    }

    close(server_fd);
    wave_index_free(&idx);
    free(traces);
    return 0;
}
