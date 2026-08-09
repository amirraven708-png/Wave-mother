#ifndef DIMN_MESH_H
#define DIMN_MESH_H
#include <stdint.h>
#include <netinet/in.h>
typedef enum { MSG_QUERY=1, MSG_NEED=2, MSG_SUPPLY=3 } msg_t;
typedef struct __attribute__((packed)) { uint32_t magic; uint8_t type; uint64_t sender; uint64_t rhythm; double phase, amp; uint64_t val; char data[128]; } dimn_pkt;
int  dimn_init(uint16_t port);
int  dimn_send(int fd, const struct sockaddr_in *dst, uint8_t t, uint64_t sid, uint64_t rh, double ph, double amp, uint64_t v, const char *d);
int  dimn_recv(int fd, dimn_pkt *p, struct sockaddr_in *src);
#endif
// Add state sync message type (if not already)
#ifndef MSG_STATE_SYNC
#define MSG_STATE_SYNC 5
#endif
