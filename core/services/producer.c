#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
#include "wave_trace.h"
#define SHARED_DIR "data/generated"
#define TRACE_COUNT 100
int main(void) {
    size_t i;
    if (mkdir(SHARED_DIR, 0755) < 0 && errno != EEXIST) { perror("mkdir"); return 1; }
    for (i = 0; i < TRACE_COUNT; ++i) {
        wd_trace_t t; char filename[512]; FILE *f;
        memset(&t, 0, sizeof(t));
        t.rhythm = UINT64_C(0x00000000ABCD1234);
        t.phase = (double)i * 100.0;
        t.type = 1;
        snprintf(t.content, sizeof(t.content), "Hello Wave %zu", i);
        t.size = (uint32_t)strlen(t.content);
        snprintf(filename, sizeof(filename), "%s/trace_%04zu.json", SHARED_DIR, i);
        f = fopen(filename, "w");
        if (!f) { perror(filename); return 1; }
        fprintf(f, "{\"rhythm\":%" PRIu64 ",\"phase\":%.17g,\"type\":%" PRIu32 ",\"size\":%" PRIu32 ",\"content\":\"%s\"}\n",
                t.rhythm, t.phase, t.type, t.size, t.content);
        fclose(f);
    }
    printf("Producer: generated %d traces in %s\n", TRACE_COUNT, SHARED_DIR);
    return 0;
}
