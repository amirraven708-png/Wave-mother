#ifndef NEED_SIGNAL_H
#define NEED_SIGNAL_H
#include <stdint.h>
typedef struct { uint64_t req, def, ts; int act; } ns_t;
ns_t* ns_trigger(uint64_t req, uint64_t dem, uint64_t cap);
int   ns_supply(ns_t *s, uint64_t amt);
int   ns_active(const ns_t *s);
void  ns_free(ns_t *s);
#endif
