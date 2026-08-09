#ifndef RADIO_PORT_H
#define RADIO_PORT_H
#include "dimn_mesh.h"
#define MAX_PEERS 16
typedef struct { uint64_t id; struct sockaddr_in addr; int active; } endpoint_t;
typedef struct { endpoint_t p[MAX_PEERS]; size_t n; int fd; uint16_t port; } radio_t;
int radio_init(radio_t *r, uint16_t port);
int radio_reg(radio_t *r, uint64_t id, const char *ip, uint16_t port);
int radio_cast(radio_t *r, uint8_t t, uint64_t sid, uint64_t rh, double ph, double amp, uint64_t v, const char *d);
#endif
