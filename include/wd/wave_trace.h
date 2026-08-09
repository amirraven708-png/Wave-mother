#ifndef WD_WAVE_TRACE_H
#define WD_WAVE_TRACE_H
#include <stdint.h>
#include <stddef.h>
#define WD_MAX_CONTENT_SIZE 256
typedef struct {
    uint64_t rhythm;
    double phase;
    uint32_t type;
    uint32_t size;
    char content[WD_MAX_CONTENT_SIZE];
} wd_trace_t;
#endif
