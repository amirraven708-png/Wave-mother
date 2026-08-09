#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "passive_discovery.h"

int setup_multicast_sender() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    int ttl = 2;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    return sock;
}

int setup_multicast_receiver() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DISCOVERY_PORT);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    return sock;
}

void send_discovery(int sock, uint64_t node_id, uint64_t phase) {
    wave_packet_t pkt = { .node_id = node_id, .phase = phase, .type = 0 };
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
    addr.sin_port = htons(DISCOVERY_PORT);
    sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
}

void send_sync(int sock, uint64_t node_id, uint64_t phase) {
    wave_packet_t pkt = { .node_id = node_id, .phase = phase, .type = 1 };
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
    addr.sin_port = htons(DISCOVERY_PORT);
    sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
}

int recv_packet(int sock, wave_packet_t *pkt) {
    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);
    return recvfrom(sock, pkt, sizeof(*pkt), 0, (struct sockaddr*)&sender, &len);
}
