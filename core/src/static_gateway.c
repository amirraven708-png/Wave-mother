#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STATIC_PORT 8082
#define SITES_DIR   "docs/sites"

// Serve a static HTML file (no Wave Host needed)
static void serve_static(int client_fd, const char *path) {
    char filepath[512];
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", SITES_DIR);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s.html", SITES_DIR, path);
    }
    
    FILE *f = fopen(filepath, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *content = malloc(size + 1);
        fread(content, 1, size, f);
        fclose(f);
        
        char header[256];
        int hlen = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        send(client_fd, header, hlen, 0);
        send(client_fd, content, size, 0);
        free(content);
    } else {
        char *nf = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
                   "<h1>Site not found in Static Mode</h1>"
                   "<p>The Wave Host is currently offline.</p>";
        send(client_fd, nf, strlen(nf), 0);
    }
    close(client_fd);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(STATIC_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    
    printf("🌊 Static Gateway (Offline Mode) on port %d\n", STATIC_PORT);
    printf("   Serving frozen sites from %s/\n", SITES_DIR);
    printf("   No Wave Host needed – phone can be offline.\n");
    
    while (1) {
        int client = accept(server_fd, NULL, NULL);
        if (client >= 0) {
            char buf[4096] = {0};
            read(client, buf, sizeof(buf)-1);
            
            char path[256] = "/";
            char *start = strchr(buf, ' ');
            if (start) {
                start++;
                char *end = strchr(start, ' ');
                if (end) {
                    size_t len = end - start;
                    if (len < sizeof(path)) {
                        strncpy(path, start, len);
                        path[len] = '\0';
                    }
                }
            }
            serve_static(client, path);
        }
    }
    close(server_fd);
    return 0;
}
