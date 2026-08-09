#ifndef DREAMLAND_WISH_H
#define DREAMLAND_WISH_H

#include <stdint.h>

#define MAX_WISH_PAYLOAD 256
#define MAX_SOURCE_IDS    16

typedef enum {
    DREAM_RESULT_EXACT,
    DREAM_RESULT_RESONANT,
    DREAM_RESULT_COMPOSED,
    DREAM_RESULT_PENDING
} dream_result_type_t;

typedef struct {
    /* how much the caller insists on an exact match */
    double precision;         // 0.0 – 1.0
    /* how much creativity / combination is allowed */
    double ingenuity;         // 0.0 – 1.0

    /* primary wave signature */
    uint64_t rhythm;
    uint64_t rhythm_mask;     // which bits of rhythm are relevant
    double   phase;           // radians, 0 .. 2π
    double   phase_tolerance; // radians

    /* optional payload (semantic hint) */
    char   payload[MAX_WISH_PAYLOAD];
    int    payload_len;

    /* temporal constraints */
    uint64_t desired_time;    // 0 = now
    uint64_t time_window;     // ns, 0 = unlimited

    /* provenance */
    uint64_t parent_transmission; // 0 = none
} wish_t;

typedef struct {
    dream_result_type_t type;
    double confidence;
    double resonance;
    uint64_t source_ids[MAX_SOURCE_IDS];
    int    source_count;
    int    persistent;       // 1 = exact source, 0 = composed / pending
    int    transmitted;      // 1 = was actually transmitted
    char   content[512];
} dream_result_t;

#endif
