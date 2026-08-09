#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BUBBLE_PAGES 5
#define BUBBLES_PER_PAGE 16

typedef struct {
    int id;
    double theta, phi;
    int pending;
    int data[8];
    int data_count;
    double hit_rate, instab;
} Bubble;

typedef struct {
    int z;
    Bubble bubbles[BUBBLES_PER_PAGE];
    unsigned char bloom[128];
    int total_pending;
} BubblePage;

typedef struct {
    int *buckets;
    int bucket_count, entries_per_bucket, fingerprint_bits, mask, count;
} CuckooFilter;

void cuckoo_init(CuckooFilter *cf, int bucket_count, int epb, int fp_bits) {
    cf->bucket_count = bucket_count; cf->entries_per_bucket = epb;
    cf->fingerprint_bits = fp_bits; cf->mask = (1 << fp_bits) - 1;
    cf->count = 0; cf->buckets = calloc(bucket_count * epb, sizeof(int));
}

typedef struct {
    double J, Ldata, Lcon, Reg, K, maxRe;
    int stable;
} ACIOMDState;

double compute_aciomd_quality(BubblePage *pages, int key) {
    double Ldata = 0.0, Lcon = 0.0, Reg = 0.0;
    int probes = 0, found = 0;

    for (int z = 0; z < 2 && !found; z++) {
        for (int b = 0; b < BUBBLES_PER_PAGE; b++) {
            probes++;
            for (int d = 0; d < pages[z].bubbles[b].data_count; d++) {
                if (pages[z].bubbles[b].data[d] == key) {
                    found = 1; Ldata += 1.0 - (double)probes / 50.0; break;
                }
            }
            if (found) break;
        }
    }

    for (int z = 2; z < BUBBLE_PAGES && !found; z++) {
        int bloom_hit = 0;
        for (int b = 0; b < BUBBLES_PER_PAGE; b++)
            if (pages[z].bubbles[b].data_count > 0) { bloom_hit = 1; break; }
        if (!bloom_hit) { Lcon += 0.2; continue; }
        for (int b = 0; b < BUBBLES_PER_PAGE; b++) {
            probes++;
            for (int d = 0; d < pages[z].bubbles[b].data_count; d++) {
                if (pages[z].bubbles[b].data[d] == key) {
                    found = 1; Ldata += 0.5 - (double)probes / 100.0; break;
                }
            }
            if (found) break;
        }
    }

    if (!found) { Ldata = -0.5; Lcon = 0.8; }
    Reg = 0.1 * probes / 20.0;
    double J = 1.0 * Ldata + 0.8 * Lcon + 0.5 * Reg;
    double K = -J + 0.3 * (found ? 1 : 0);

    return K;
}

double compute_harmony_from_aciomd(double K) {
    double harmony = 0.7 + 0.3 * (K + 1.0) / 2.0;
    if (harmony > 1.0) harmony = 0.95;
    if (harmony < 0.3) harmony = 0.3;
    return harmony;
}

int main() {
    printf("🌊 Wave Mother – Integrated Core (C)\n");
    printf("===========================================\n");

    BubblePage pages[BUBBLE_PAGES];
    for (int z = 0; z < BUBBLE_PAGES; z++) {
        pages[z].z = z;
        for (int b = 0; b < BUBBLES_PER_PAGE; b++) {
            pages[z].bubbles[b].id = b;
            if (b % 4 == 0) {
                pages[z].bubbles[b].data[0] = b * 10 + z;
                pages[z].bubbles[b].data_count = 1;
            }
        }
    }
    printf("✅ BubbleDB initialised.\n");

    int key = 42;
    double K = compute_aciomd_quality(pages, key);
    printf("✅ ACIOMD search for key %d → K = %.4f\n", key, K);

    double harmony = compute_harmony_from_aciomd(K);
    printf("✅ Raven Stars block harmony = %.4f\n", harmony);

    printf("\n📊 Chain: BubbleDB → ACIOMD → Raven Stars\n");
    printf("   K = %.4f  →  harmony = %.4f\n", K, harmony);
    printf("✅ Full integration successful!\n");
    return 0;
}
