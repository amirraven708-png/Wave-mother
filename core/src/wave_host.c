#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wave_trace.h"
#include "wave_index.h"

#define HTTP_PORT 8080
#define BUF_SIZE 32768
#define MAX_CONTENT (4096)
#define SITES_DIR "data/sites"

static wave_index_t *g_idx = NULL;
static wd_trace_t   *g_traces = NULL;
static size_t        g_count = 0;

// Simple string hash -> rhythm
static uint64_t subdomain_hash(const char *sub) {
    uint64_t h = 0x57415645; // "WAVE"
    for (const char *p = sub; *p; p++)
        h = (h << 5) + h + *p;
    return h;
}

// Read file content (up to MAX_CONTENT)
static char *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > MAX_CONTENT) sz = MAX_CONTENT;
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t n = fread(buf, 1, sz, f);
    buf[n] = '\0';
    fclose(f);
    *out_size = n;
    return buf;
}

// Rebuild index by scanning data/sites/
static void rebuild_index() {
    // Free previous
    wave_index_free(g_idx);
    free(g_traces);
    g_traces = NULL;
    g_count = 0;

    DIR *d = opendir(SITES_DIR);
    if (!d) {
        mkdir(SITES_DIR, 0755);
        d = opendir(SITES_DIR);
        if (!d) return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type != DT_REG) continue;
        char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".html") != 0) continue;

        // subdomain = filename without .html
        size_t len = dot - ent->d_name;
        char sub[64];
        if (len >= sizeof(sub)) len = sizeof(sub)-1;
        strncpy(sub, ent->d_name, len);
        sub[len] = '\0';

        uint64_t rhythm = subdomain_hash(sub);
        g_traces = realloc(g_traces, (g_count+1)*sizeof(wd_trace_t));
        wd_trace_t *t = &g_traces[g_count];
        memset(t, 0, sizeof(*t));
        t->exact_key = rhythm;
        t->rhythm = rhythm;
        t->phase = 1000.0 + g_count;  // dummy
        t->type = 1;
        t->size = 0; // not used for serving
        g_count++;
    }
    closedir(d);

    g_idx = malloc(sizeof(wave_index_t));
    wave_index_build(g_idx, g_traces, g_count, 256);
    printf("Index rebuilt: %zu sites found\n", g_count);
}

// Add a new site (subdomain + html file)
static int add_site(const char *sub, const char *html, size_t html_len) {
    // Validate subdomain: only letters and digits
    for (const char *p = sub; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')))
            return -1;

    // Check if already exists
    char path[256];
    snprintf(path, sizeof(path), SITES_DIR "/%s.html", sub);
    if (access(path, F_OK) == 0) return -2; // already exists

    // Write the HTML file
    FILE *f = fopen(path, "w");
    if (!f) return -3;
    fwrite(html, 1, html_len, f);
    fclose(f);

    // Add to index
    uint64_t rhythm = subdomain_hash(sub);
    wd_trace_t *t = realloc(g_traces, (g_count+1)*sizeof(wd_trace_t));
    memset(t, 0, sizeof(*t));
    t->exact_key = rhythm;
    t->rhythm = rhythm;
    t->phase = 1000.0 + g_count;
    t->type = 1;
    g_count++;
    wave_index_free(g_idx);
    g_idx = malloc(sizeof(wave_index_t));
    wave_index_build(g_idx, g_traces, g_count, 256);
    return 0;
}

// Parse POST body for form data (x-www-form-urlencoded)
static int parse_form(const char *body, char *sub, size_t sub_len, char *html, size_t html_len) {
    // Find subdomain=
    const char *p = strstr(body, "subdomain=");
    if (!p) return -1;
    p += 10;
    const char *end = strchr(p, '&');
    if (!end) end = p + strlen(p);
    size_t len = end - p;
    if (len >= sub_len) len = sub_len-1;
    strncpy(sub, p, len);
    sub[len] = '\0';

    // URL-decode subdomain (just spaces maybe)
    // Skip for now.

    // Find html=
    p = strstr(body, "html=");
    if (!p) return -1;
    p += 5;
    // The rest is the HTML, possibly URL-encoded
    // Simple: copy until end of body
    size_t hlen = strlen(p);
    if (hlen >= html_len) hlen = html_len-1;
    strncpy(html, p, hlen);
    html[hlen] = '\0';

    // URL decode %xx
    char decoded[html_len];
    size_t dlen = 0;
    for (size_t i=0; i<hlen; i++) {
        if (p[i] == '%' && i+2 < hlen) {
            int hex;
            sscanf(p+i+1, "%2x", &hex);
            decoded[dlen++] = (char)hex;
            i += 2;
        } else if (p[i] == '+') {
            decoded[dlen++] = ' ';
        } else {
            decoded[dlen++] = p[i];
        }
    }
    decoded[dlen] = '\0';
    strncpy(html, decoded, html_len);
    return 0;
}

// Handle HTTP request
static void handle_request(int client_fd) {
    char buf[BUF_SIZE] = {0};
    read(client_fd, buf, BUF_SIZE-1);

    // Extract method, path, Host header
    char method[8] = {0};
    char path[256] = "/";
    char host[256] = "";
    sscanf(buf, "%7s %255s", method, path);

    char *host_line = strstr(buf, "Host: ");
    if (host_line) {
        host_line += 6;
        sscanf(host_line, "%255s", host);
        char *e = strpbrk(host, "\r\n"); if (e) *e = '\0';
    }

    // Determine subdomain
    char sub[64] = "";
    if (strstr(host, ".w.END.d")) {
        char *dot = strstr(host, ".w.END.d");
        size_t len = dot - host;
        if (len > 0 && len < sizeof(sub)) {
            strncpy(sub, host, len);
            sub[len] = '\0';
        }
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/create") == 0) {
        // Find body
        char *body = strstr(buf, "\r\n\r\n");
        if (body) body += 4;
        else { close(client_fd); return; }

        char new_sub[64], new_html[4096];
        if (parse_form(body, new_sub, sizeof(new_sub), new_html, sizeof(new_html)) == 0) {
            int res = add_site(new_sub, new_html, strlen(new_html));
            if (res == 0) {
                char response[512];
                snprintf(response, sizeof(response),
                    "HTTP/1.1 302 Found\r\nLocation: http://%s.w.END.d:%d/\r\n\r\n",
                    new_sub, HTTP_PORT);
                send(client_fd, response, strlen(response), 0);
            } else {
                char *msg = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nSubdomain already taken or invalid";
                send(client_fd, msg, strlen(msg), 0);
            }
        } else {
            char *msg = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid form";
            send(client_fd, msg, strlen(msg), 0);
        }
        close(client_fd);
        return;
    }

    // GET request
    if (strlen(sub) == 0 || strcmp(sub, "www") == 0) {
        // Serve platform page
        size_t sz;
        char *content = read_file("site/platform.html", &sz);
        if (content) {
            char header[256];
            int hlen = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            send(client_fd, header, hlen, 0);
            send(client_fd, content, sz, 0);
            free(content);
        }
        close(client_fd);
        return;
    }

    // User subdomain
    uint64_t rhythm = subdomain_hash(sub);
    wd_trace_t out;
    int found = wave_index_get_by_key(g_idx, rhythm, &out);
    if (found) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), SITES_DIR "/%s.html", sub);
        size_t sz;
        char *content = read_file(filepath, &sz);
        if (content) {
            char header[256];
            int hlen = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            send(client_fd, header, hlen, 0);
            send(client_fd, content, sz, 0);
            free(content);
        } else {
            char *msg = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            send(client_fd, msg, strlen(msg), 0);
        }
    } else {
        size_t sz;
        char *content = read_file("site/notfound.html", &sz);
        if (content) {
            char header[256];
            int hlen = snprintf(header, sizeof(header), "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n");
            send(client_fd, header, hlen, 0);
            send(client_fd, content, sz, 0);
            free(content);
        }
    }
    close(client_fd);
}

int main() {
    mkdir(SITES_DIR, 0755);
    rebuild_index();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("\n🌊 Wave Hosting Platform\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Create your site: http://localhost:%d\n", HTTP_PORT);
    printf("Your site:         http://<name>.w.END.d:%d\n", HTTP_PORT);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client >= 0) handle_request(client);
    }
    close(server_fd);
    wave_index_free(g_idx);
    free(g_traces);
    return 0;
}
