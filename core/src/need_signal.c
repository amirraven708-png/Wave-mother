#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "need_signal.h"
ns_t* ns_trigger(uint64_t req, uint64_t dem, uint64_t cap){if(dem<=cap)return NULL;ns_t *s=malloc(sizeof(ns_t));s->req=req;s->def=dem-cap;s->ts=(uint64_t)time(NULL);s->act=1;printf("NEED SIGNAL %lu deficit=%lu\n",req,s->def);return s;}
int ns_supply(ns_t *s, uint64_t amt){if(!s||!s->act)return 0;if(amt>=s->def){s->def=0;s->act=0;printf("RESOLVED\n");return 1;}s->def-=amt;printf("Partial, rem=%lu\n",s->def);return 0;}
int ns_active(const ns_t *s){return s&&s->act;}
void ns_free(ns_t *s){free(s);}
