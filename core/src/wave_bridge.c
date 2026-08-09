#include <stdint.h>
int dimn_init_socket(uint16_t port);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "dimn_mesh.h"
#include "wave_trace.h"
#include "wave_index.h"

#define BRIDGE_PORT 8080
#define DIMN_PORT   9090
#define BUF_SIZE    16384

static const char *NOT_FOUND = 
    "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
    "<h1>Site not found in Wave Network</h1>"
    "<p>This subdomain has not been registered yet.</p>"
    "<p><a href='http://w.END.d:%d'>Create your own site</a></p>";

static const char *HOME_PAGE = 
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<!DOCTYPE html><html><head><title>Wave Network</title></head>"
    "<body style='font-family:sans-serif;background:#0a0a1a;color:#ddd;padding:40px;text-align:center'>"
    "<h1 style='color:#00dcff'>🌊 Wave DIMN Network</h1>"
    "<p>Your node is part of the decentralized Wave network.</p>"
    "<form method='POST' action='/create'>"
    "<input name='subdomain' placeholder='yoursite' pattern='[a-zA-Z0-9]+' required>"
    "<textarea name='html' placeholder='<h1>Hello Wave</h1>' required></textarea>"
    "<button type='submit'>Create Site</button>"
    "</form></body></html>";

int main(int argc, char **argv) {
    int dimn_fd, server_fd;
    struct sockaddr_in server_addr;
    
    // ۱. اتصال به شبکه DIMN
    dimn_fd = dimn_init_socket(DIMN_PORT);
    if (dimn_fd < 0) {
        fprintf(stderr, "Failed to initialize DIMN on port %d\n", DIMN_PORT);
        return 1;
    }
    printf("[Wave Bridge] Connected to DIMN on UDP %d\n", DIMN_PORT);
    
    // ۲. راه‌اندازی سرور HTTP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BRIDGE_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);
    
    printf("[Wave Bridge] HTTP → DIMN Bridge on port %d\n", BRIDGE_PORT);
    printf("[Wave Bridge] This node is now a gateway to the Wave network.\n");
    printf("[Wave Bridge] Other nodes can find your sites via DIMN.\n\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;
        
        char buf[BUF_SIZE] = {0};
        read(client_fd, buf, BUF_SIZE-1);
        
        // استخراج Host header
        char host[256] = "";
        char *host_line = strstr(buf, "Host: ");
        if (host_line) {
            host_line += 6;
            sscanf(host_line, "%255s", host);
            char *e = strpbrk(host, "\r\n"); if (e) *e = '\0';
        }
        
        // استخراج subdomain
        char subdomain[64] = "";
        if (strstr(host, ".w.END.d")) {
            char *dot = strstr(host, ".w.END.d");
            size_t len = dot - host;
            if (len > 0 && len < sizeof(subdomain)) {
                strncpy(subdomain, host, len);
                subdomain[len] = '\0';
            }
        }
        
        if (strlen(subdomain) == 0 || strcmp(subdomain, "www") == 0) {
            // صفحه اصلی
            send(client_fd, HOME_PAGE, strlen(HOME_PAGE), 0);
        } else {
            // ارسال کوئری به شبکه DIMN برای پیدا کردن سایت
            dimn_pkt query;
            memset(&query, 0, sizeof(query));
            query.magic = 0x57415645;
            query.type = MSG_QUERY;
            query.sender = 0;
            uint64_t rhythm = 0x57415645;
            for (const char *p = subdomain; *p; p++)
                rhythm = (rhythm << 5) + rhythm + *p;
            query.rhythm = rhythm;
            strncpy(query.data, subdomain, sizeof(query.data)-1);
            
            struct sockaddr_in bcast;
            bcast.sin_family = AF_INET;
            bcast.sin_port = htons(DIMN_PORT);
            bcast.sin_addr.s_addr = inet_addr("127.0.0.1");
            sendto(dimn_fd, &query, sizeof(query), 0, (struct sockaddr*)&bcast, sizeof(bcast));
            
            struct timeval tv = {1, 0};
            setsockopt(dimn_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            dimn_pkt response;
            struct sockaddr_in responder;
            socklen_t resp_len = sizeof(responder);
            
            int n = recvfrom(dimn_fd, &response, sizeof(response), 0, 
                           (struct sockaddr*)&responder, &resp_len);
            
            if (n > 0 && response.magic == 0x57415645 && response.type == MSG_SUPPLY) {
                char reply[512];
                snprintf(reply, sizeof(reply),
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                    "<h1>🌊 Site found on Wave Network</h1>"
                    "<p>Subdomain: <strong>%s</strong></p>"
                    "<p>Served by node: <strong>0x%lX</strong></p>"
                    "<p>Rhythm: <strong>0x%lX</strong></p>"
                    "<hr><p style='color:#00dcff'>This page was delivered via the DIMN mesh network.</p>",
                    subdomain, response.sender, response.rhythm);
                send(client_fd, reply, strlen(reply), 0);
            } else {
                char reply[1024];
                snprintf(reply, sizeof(reply), NOT_FOUND, BRIDGE_PORT);
                send(client_fd, reply, strlen(reply), 0);
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    close(dimn_fd);
    return 0;
}
