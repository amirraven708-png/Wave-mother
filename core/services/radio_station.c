#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include "wave_trace.h"
#include "wave_index.h"
#include "need_signal.h"
#include "radio_port.h"
#define K 0.05
static wi idx; static wd_trace_t *t; static size_t tc; static int fd; static radio_t rp;
static double ph=0.0, fq=1.0; static uint64_t nid=0xAAAA1111; static ns_t *sig=NULL;
void inj(uint64_t rh, double p, const char *c){t=realloc(t,(tc+1)*sizeof(wd_trace_t));t[tc].rhythm=rh;t[tc].phase=p;t[tc].type=1;t[tc].size=strlen(c);strcpy(t[tc].content,c);tc++;wi_build(&idx,t,tc,256);}
void proc(void){dimn_pkt p;struct sockaddr_in src;if(dimn_recv(rp.fd,&p,&src)==0){if(p.sender==nid)return;if(p.type==MSG_NEED){printf("[Mesh] Need from %" PRIX64 " def=%" PRIu64 "\n",p.sender,p.val);uint64_t sup=50;dimn_send(rp.fd,&src,MSG_SUPPLY,nid,(uint64_t)nid,ph,1.0,sup,"SUPPLY");}else if(p.type==MSG_SUPPLY){printf("[Mesh] Supply +%" PRIu64 " from %" PRIX64 "\n",p.val,p.sender);if(sig&&ns_active(sig)){ns_supply(sig,p.val);if(!ns_active(sig)){ns_free(sig);sig=NULL;}}}}}
int main(int argc, char **argv){uint16_t port=9090,pport=9091;if(argc>1)nid=strtoull(argv[1],NULL,16);if(argc>2)port=atoi(argv[2]);if(argc>3)pport=atoi(argv[3]);
  signal(SIGPIPE,SIG_IGN);radio_init(&rp,port);radio_reg(&rp,0xBBBB2222,"127.0.0.1",pport);
  printf("Radio Node %" PRIX64 " on UDP %d\n",nid,port);
  int cyc=0;
  while(1){ph+=fq*5.0;if(ph>=360.0)ph-=360.0;inj(nid,ph,"data");
    if(tc<300&&!sig){sig=ns_trigger(nid,300,tc);radio_cast(&rp,MSG_NEED,nid,(uint64_t)nid,ph,1.0,sig->def,"DEF");}
    else if(sig&&tc>=300){ns_supply(sig,100);if(!ns_active(sig)){ns_free(sig);sig=NULL;}}
    fd_set rf;struct timeval tv={0,100000};FD_ZERO(&rf);FD_SET(rp.fd,&rf);
    if(select(rp.fd+1,&rf,NULL,NULL,&tv)>0)proc();
    cyc++;usleep(400000);}
  return 0;}
