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
