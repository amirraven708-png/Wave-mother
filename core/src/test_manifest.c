#include <stdio.h>
#include "manifest_volumes.h"

int main() {
    BlueBusSolidCore core;
    manifest_init(&core);
    trace_all_manifest_paths(&core);
    int frozen = apply_throat_compression(&core);
    if (frozen) check_observer_sync(&core);
    printf("\n=== MANIFEST STATUS ===\n");
    printf("Volumes: %d\n", NUM_VOLUMES);
    printf("Paths traced: %d\n", core.path_count);
    printf("Time frozen: %s\n", core.is_frozen ? "YES" : "NO");
    printf("Solid traces: %zu\n", core.solid_count);
    return 0;
}
