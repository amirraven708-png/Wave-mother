#ifndef WAVE_TRACE_H
#define WAVE_TRACE_H
#include <stdint.h>
typedef struct { uint64_t exact_key, rhythm; double phase; uint32_t type, size; char content[4096]; } wd_trace_t;
#endif
