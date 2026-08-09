#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "license_manager.h"
#define LICENSE_DIR "./licenses"
#define DEFAULT_REPUTATION 70.0
#define DECAY_RATE 0.1
static void ensure_dir(void) { mkdir(LICENSE_DIR, 0755); }
void lm_init(void) { ensure_dir(); }
static void write_license(uint64_t node_id, double rep, int active) {
    char path[256]; snprintf(path, sizeof(path), LICENSE_DIR "/%lu.json", node_id);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "{\"node_id\":%lu,\"reputation\":%.2f,\"active\":%d}\n", node_id, rep, active); fclose(f); }
}
int lm_issue_license(uint64_t node_id) {
    char path[256]; snprintf(path, sizeof(path), LICENSE_DIR "/%lu.json", node_id);
    if (access(path, F_OK) == 0) return 0;
    write_license(node_id, DEFAULT_REPUTATION, 1);
    return 1;
}
int lm_renew_license(uint64_t node_id) { write_license(node_id, DEFAULT_REPUTATION, 1); return 1; }
void lm_update_reputation(uint64_t node_id, double delta) {
    char path[256]; snprintf(path, sizeof(path), LICENSE_DIR "/%lu.json", node_id);
    FILE *f = fopen(path, "r");
    double rep = DEFAULT_REPUTATION; int active = 1;
    if (f) { fscanf(f, "{\"node_id\":%*lu,\"reputation\":%lf,\"active\":%d}", &rep, &active); fclose(f); }
    rep += delta; if (rep < 0) rep = 0; if (rep > 100) rep = 100;
    active = (rep >= 20.0) ? 1 : 0;
    write_license(node_id, rep, active);
}
void lm_decay_all(void) {
    DIR *d = opendir(LICENSE_DIR); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strstr(e->d_name, ".json")) continue;
        uint64_t node_id = strtoull(e->d_name, NULL, 10);
        lm_update_reputation(node_id, -DECAY_RATE);
    }
    closedir(d);
}
void lm_print_status(void) {
    DIR *d = opendir(LICENSE_DIR); if (!d) return;
    struct dirent *e;
    printf("=== License Status ===\n");
    while ((e = readdir(d)) != NULL) {
        if (!strstr(e->d_name, ".json")) continue;
        uint64_t node_id = strtoull(e->d_name, NULL, 10);
        char path[256]; snprintf(path, sizeof(path), LICENSE_DIR "/%lu.json", node_id);
        FILE *f = fopen(path, "r");
        if (f) {
            double rep; int active;
            fscanf(f, "{\"node_id\":%*lu,\"reputation\":%lf,\"active\":%d}", &rep, &active);
            printf("Node %lu: rep=%.2f active=%d\n", node_id, rep, active);
            fclose(f);
        }
    }
    closedir(d);
}
