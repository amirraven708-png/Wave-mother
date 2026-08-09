#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SOCK_PATH "run/wave_listener.sock"
int main(int argc, char **argv) {
    int sock; struct sockaddr_un addr; char buffer[65536]; ssize_t n;
    if (argc != 2) { fprintf(stderr, "Usage:\n  %s we://<rhythm>\n  %s we://<rhythm>/<min>-<max>\n", argv[0], argv[0]); return 1; }
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); close(sock); return 1; }
    if (write(sock, argv[1], strlen(argv[1]) + 1) < 0) { perror("write"); close(sock); return 1; }
    while ((n = read(sock, buffer, sizeof(buffer) - 1)) > 0) { buffer[n] = '\0'; fputs(buffer, stdout); }
    close(sock);
    return 0;
}
