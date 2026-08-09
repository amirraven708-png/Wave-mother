#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include "wave_trace.h"

#define SHARED_DIR "./data/generated"

int main(void) {
    system("mkdir -p " SHARED_DIR);
    srand(time(NULL));
    
    for (int i = 0; i < 100; i++) {
        wd_trace_t t;
        t.rhythm = 0xABCD1234;
        t.phase = (double)(i * 100);
        t.type = 1;
        t.size = 12;
        strcpy(t.content, "Hello Wave!");
        
        char fname[256];
        snprintf(fname, sizeof(fname), "%s/trace_%04d.json", SHARED_DIR, i);
        FILE *f = fopen(fname, "w");
        if (f) {
            fprintf(f, "{\"rhythm\":%" PRIu64 ",\"phase\":%f,\"type\":%u,\"size\":%u,\"content\":\"%s\"}\n",
                    t.rhythm, t.phase, t.type, t.size, t.content);
            fclose(f);
            printf("Produced %s (phase=%f)\n", fname, t.phase);
        }
    }
    return 0;
}
