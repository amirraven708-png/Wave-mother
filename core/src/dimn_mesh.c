#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "dimn_mesh.h"
int dimn_init(uint16_t port){int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;int r=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&r,sizeof(r));struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port),.sin_addr.s_addr=INADDR_ANY};if(bind(fd,(struct sockaddr*)&a,sizeof(a))<0){close(fd);return -1;}return fd;}
int dimn_send(int fd, const struct sockaddr_in *dst, uint8_t t, uint64_t sid, uint64_t rh, double ph, double amp, uint64_t v, const char *d){dimn_pkt p;memset(&p,0,sizeof(p));p.magic=0x57415645;p.type=t;p.sender=sid;p.rhythm=rh;p.phase=ph;p.amp=amp;p.val=v;if(d)strncpy(p.data,d,sizeof(p.data)-1);return sendto(fd,&p,sizeof(p),0,(struct sockaddr*)dst,sizeof(*dst));}
int dimn_recv(int fd, dimn_pkt *p, struct sockaddr_in *src){socklen_t al=sizeof(struct sockaddr_in);ssize_t n=recvfrom(fd,p,sizeof(dimn_pkt),0,(struct sockaddr*)src,&al);if(n<(ssize_t)sizeof(dimn_pkt))return -1;if(p->magic!=0x57415645)return -2;return 0;}
