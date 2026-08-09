#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "wave_trace.h"

#define SHARED_DIR "./shared_dir"
#define MY_RHYTHM 0xABCD1234

int main() {
    system("mkdir -p " SHARED_DIR);
    printf("Consumer started. Listening for traces with rhythm 0x%lX...\n", MY_RHYTHM);

    while (1) {
        DIR *dir = opendir(SHARED_DIR);
        if (!dir) {
            sleep(1);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".json") == NULL) continue;

            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", SHARED_DIR, entry->d_name);

            FILE *f = fopen(filepath, "r");
            if (!f) continue;

            wave_trace_t trace;
            char cont[64];
            int matched = fscanf(f, "{\"rhythm\":%lu,\"phase\":%lu,\"type\":%u,\"size\":%u,\"content\":\"%[^\"]\"}",
                                 &trace.rhythm, &trace.phase, &trace.type, &trace.size, cont);
            fclose(f);

            if (matched == 5 && trace.rhythm == MY_RHYTHM) {
                printf(">> SELECTED: phase=%lu, content=\"%s\"\n", trace.phase, cont);
                remove(filepath);
            } else {
                printf("Ignored: rhythm=0x%lX (not mine)\n", trace.rhythm);
                remove(filepath);
            }
        }
        closedir(dir);
        usleep(500000);
    }
    return 0;
}
