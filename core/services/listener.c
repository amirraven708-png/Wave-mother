#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "wave_trace.h"
#include "wave_index.h"
#define SHARED_DIR "data/generated"
#define UNIX_SOCK  "run/wave_listener.sock"
#define HTTP_HOST  "127.0.0.1"
#define HTTP_PORT  8080
#define MAX_TRACES 100000
#define MAX_RESULTS 1024
#define REQUEST_SIZE 4096
static wave_index_t index_db;
static wd_trace_t *traces = NULL;
static size_t trace_count = 0;
static volatile sig_atomic_t running = 1;
static void stop_server(int sig) { (void)sig; running = 0; }
static int parse_trace_file(const char *path, wd_trace_t *out) {
    FILE *f; char content[WD_MAX_CONTENT_SIZE];
    if (!path || !out) return -1;
    memset(out, 0, sizeof(*out));
    f = fopen(path, "r");
    if (!f) return -1;
    memset(content, 0, sizeof(content));
    int n = fscanf(f, "{\"rhythm\":%" SCNu64 ",\"phase\":%lf,\"type\":%" SCNu32 ",\"size\":%" SCNu32 ",\"content\":\"%255[^\"]\"}",
                   &out->rhythm, &out->phase, &out->type, &out->size, content);
    fclose(f);
    if (n != 5) return -1;
    strncpy(out->content, content, sizeof(out->content) - 1);
    out->content[sizeof(out->content) - 1] = '\0';
    return 0;
}
static int load_traces(void) {
    DIR *dir; struct dirent *entry;
    size_t capacity = 256;
    traces = calloc(capacity, sizeof(wd_trace_t));
    if (!traces) return -1;
    dir = opendir(SHARED_DIR);
    if (!dir) { fprintf(stderr, "Listener: cannot open %s: %s\n", SHARED_DIR, strerror(errno)); free(traces); traces = NULL; return -1; }
    while ((entry = readdir(dir)) != NULL) {
        char path[512]; wd_trace_t t;
        if (!strstr(entry->d_name, ".json")) continue;
        if (trace_count >= MAX_TRACES) break;
        snprintf(path, sizeof(path), "%s/%s", SHARED_DIR, entry->d_name);
        if (parse_trace_file(path, &t) != 0) continue;
        if (trace_count >= capacity) {
            size_t new_capacity = capacity * 2;
            if (new_capacity > MAX_TRACES) new_capacity = MAX_TRACES;
            wd_trace_t *tmp = realloc(traces, new_capacity * sizeof(wd_trace_t));
            if (!tmp) { closedir(dir); free(traces); traces = NULL; trace_count = 0; return -1; }
            traces = tmp; capacity = new_capacity;
        }
        traces[trace_count++] = t;
    }
    closedir(dir);
    if (trace_count == 0) { fprintf(stderr, "Listener: no valid traces found.\n"); free(traces); traces = NULL; return -1; }
    if (wave_index_build(&index_db, traces, trace_count, 256) != 0) {
        fprintf(stderr, "Listener: index build failed.\n");
        free(traces); traces = NULL; trace_count = 0; return -1;
    }
    fprintf(stderr, "Listener: loaded %zu traces.\n", trace_count);
    return 0;
}
static int parse_we_target(const char *input, uint64_t *rhythm, double *phase_min, double *phase_max) {
    const char *p; char rhythm_hex[17]; char phase_part[128]; const char *slash;
    if (!input || !rhythm || !phase_min || !phase_max) return -1;
    *rhythm = 0; *phase_min = 0.0; *phase_max = 1.0e30;
    if (strncmp(input, "we://", 5) != 0) return -1;
    p = input + 5;
    slash = strchr(p, '/');
    if (slash) {
        size_t len = (size_t)(slash - p);
        if (len == 0 || len > 16) return -1;
        memcpy(rhythm_hex, p, len); rhythm_hex[len] = '\0';
        snprintf(phase_part, sizeof(phase_part), "%s", slash + 1);
        if (sscanf(phase_part, "%lf-%lf", phase_min, phase_max) != 2) return -1;
    } else {
        size_t len = strlen(p);
        if (len == 0 || len > 16) return -1;
        memcpy(rhythm_hex, p, len); rhythm_hex[len] = '\0';
    }
    char *end = NULL;
    *rhythm = strtoull(rhythm_hex, &end, 16);
    if (!end || *end != '\0') return -1;
    if (*phase_min > *phase_max) return -1;
    return 0;
}
static int resolve_target(const char *target, char *output, size_t output_size) {
    uint64_t rhythm; double phase_min, phase_max; wd_trace_t results[MAX_RESULTS];
    int n; size_t used = 0; int i;
    if (parse_we_target(target, &rhythm, &phase_min, &phase_max) != 0) {
        snprintf(output, output_size, "ERROR invalid we:// target\n"); return -1;
    }
    n = wave_index_select(&index_db, rhythm, UINT64_MAX, phase_min, phase_max, results, MAX_RESULTS);
    if (n <= 0) { snprintf(output, output_size, "WD-RESOLVE: no-match\n"); return 0; }
    for (i = 0; i < n; ++i) {
        int written = snprintf(output + used, output_size - used,
                               "rhythm=%" PRIX64 " phase=%.6f type=%" PRIu32 " size=%" PRIu32 " content=%s\n",
                               results[i].rhythm, results[i].phase, results[i].type, results[i].size, results[i].content);
        if (written < 0) break;
        if ((size_t)written >= output_size - used) { used = output_size - 1; break; }
        used += (size_t)written;
    }
    return n;
}
static void http_response(int fd, int status, const char *body) {
    const char *status_text = status == 200 ? "OK" : status == 400 ? "Bad Request" : "Internal Server Error";
    dprintf(fd, "HTTP/1.1 %d %s\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
            status, status_text, strlen(body), body);
}
static void handle_http(int fd) {
    char request[REQUEST_SIZE]; char target[512]; char response[65536]; ssize_t n;
    memset(request, 0, sizeof(request));
    n = read(fd, request, sizeof(request) - 1);
    if (n <= 0) return;
    if (sscanf(request, "GET %511s HTTP/", target) != 1) { http_response(fd, 400, "Invalid HTTP request\n"); return; }
    if (strcmp(target, "/health") == 0) { http_response(fd, 200, "WD listener: OK\n"); return; }
    if (strncmp(target, "/we/", 4) != 0) { http_response(fd, 400, "Use /we/<rhythm>[/<phase-min>-<phase-max>]\n"); return; }
    char we_target[512]; snprintf(we_target, sizeof(we_target), "we://%s", target + 4);
    memset(response, 0, sizeof(response));
    if (resolve_target(we_target, response, sizeof(response)) < 0) { http_response(fd, 400, response); return; }
    http_response(fd, 200, response);
}
static int create_unix_socket(void) {
    int fd; struct sockaddr_un addr;
    unlink(UNIX_SOCK);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_SOCK, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 64) < 0) { close(fd); return -1; }
    return fd;
}
static int create_http_socket(void) {
    int fd; int yes = 1; struct sockaddr_in addr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_PORT);
    if (inet_pton(AF_INET, HTTP_HOST, &addr.sin_addr) != 1) { close(fd); return -1; }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 128) < 0) { close(fd); return -1; }
    return fd;
}
static void handle_unix(int fd) {
    char request[512]; char response[65536]; ssize_t n;
    memset(request, 0, sizeof(request));
    n = read(fd, request, sizeof(request) - 1);
    if (n <= 0) return;
    memset(response, 0, sizeof(response));
    resolve_target(request, response, sizeof(response));
    write(fd, response, strlen(response));
}
int main(void) {
    int unix_fd, http_fd;
    signal(SIGINT, stop_server); signal(SIGTERM, stop_server); signal(SIGPIPE, SIG_IGN);
    if (load_traces() != 0) return 1;
    unix_fd = create_unix_socket();
    if (unix_fd < 0) { perror("unix socket"); wave_index_free(&index_db); free(traces); return 1; }
    http_fd = create_http_socket();
    if (http_fd < 0) { perror("http socket"); close(unix_fd); unlink(UNIX_SOCK); wave_index_free(&index_db); free(traces); return 1; }
    fprintf(stderr, "WD listener started\n  UNIX: %s\n  HTTP: http://%s:%d\n", UNIX_SOCK, HTTP_HOST, HTTP_PORT);
    while (running) {
        fd_set readfds; int maxfd; FD_ZERO(&readfds); FD_SET(unix_fd, &readfds); FD_SET(http_fd, &readfds);
        maxfd = unix_fd > http_fd ? unix_fd : http_fd;
        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        int r = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; perror("select"); break; }
        if (r == 0) continue;
        if (FD_ISSET(unix_fd, &readfds)) {
            int client = accept(unix_fd, NULL, NULL);
            if (client >= 0) { handle_unix(client); close(client); }
        }
        if (FD_ISSET(http_fd, &readfds)) {
            int client = accept(http_fd, NULL, NULL);
            if (client >= 0) { handle_http(client); close(client); }
        }
    }
    close(unix_fd); close(http_fd); unlink(UNIX_SOCK);
    wave_index_free(&index_db); free(traces);
    fprintf(stderr, "WD listener stopped.\n");
    return 0;
}
