#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "radio_port.h"
int radio_init(radio_t *r, uint16_t port){if(!r)return -1;r->n=0;r->port=port;r->fd=dimn_init(port);return r->fd;}
int radio_reg(radio_t *r, uint64_t id, const char *ip, uint16_t port){if(!r||r->n>=MAX_PEERS)return -1;endpoint_t *e=&r->p[r->n];e->id=id;e->active=1;memset(&e->addr,0,sizeof(e->addr));e->addr.sin_family=AF_INET;e->addr.sin_port=htons(port);e->addr.sin_addr.s_addr=inet_addr(ip);r->n++;return 0;}
int radio_cast(radio_t *r, uint8_t t, uint64_t sid, uint64_t rh, double ph, double amp, uint64_t v, const char *d){if(!r||r->fd<0)return -1;int c=0;for(size_t i=0;i<r->n;i++)if(r->p[i].active){dimn_send(r->fd,&r->p[i].addr,t,sid,rh,ph,amp,v,d);c++;}return c;}
