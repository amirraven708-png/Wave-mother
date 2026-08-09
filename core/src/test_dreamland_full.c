#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dreamland.h"

int main() {
    dreamland_t dl;
    dreamland_init(&dl);

    /* create some traces directly */
    wd_trace_t traces[] = {
        { .exact_key=1, .rhythm=0xABCD0001, .phase=1570, .type=1, .size=12, .content="Home Page" },
        { .exact_key=2, .rhythm=0xABCD0002, .phase=3141, .type=1, .size=12, .content="About Us" },
        { .exact_key=3, .rhythm=0xABCD0003, .phase=4712, .type=1, .size=12, .content="Products" },
        { .exact_key=4, .rhythm=0xBBBB0001, .phase= 785, .type=1, .size=12, .content="Contact" },
        { .exact_key=5, .rhythm=0xABCD0004, .phase=6283, .type=1, .size=12, .content="Settings" },
    };
    int n = 5;
    dl.traces = traces;
    dl.trace_count = n;

    wave_index_t idx;
    wave_index_build(&idx, traces, n, 16);
    dl.wave_index = &idx;

    printf("=== DREAMLAND FULL TEST ===\n\n");

    /* 1. Exact wish */
    wish_t w1 = { .precision=1.0, .ingenuity=0.0, .rhythm=0xABCD0001 };
    dream_result_t res1[2];
    int n1 = dreamland_wish(&dl, &w1, res1, 2);
    printf("Exact wish: found %d\n", n1);
    for (int i=0; i<n1; i++) printf("  -> %s (type=%d, persistent=%d)\n", res1[i].content, res1[i].type, res1[i].persistent);

    /* 2. Pattern wish with high precision */
    wish_t w2 = { .precision=0.9, .ingenuity=0.2, .rhythm=0xABCD0000, .rhythm_mask=0xFFFF0000,
                  .phase=M_PI/2, .phase_tolerance=1.0 };
    dream_result_t res2[3];
    int n2 = dreamland_wish(&dl, &w2, res2, 3);
    printf("\nPattern wish (ABCD*, phase~90deg): found %d\n", n2);
    for (int i=0; i<n2; i++) printf("  -> %s (conf=%.2f)\n", res2[i].content, res2[i].confidence);

    /* 3. Ingenuity wish */
    wish_t w3 = { .precision=0.2, .ingenuity=0.9, .phase=M_PI/4, .phase_tolerance=2.0,
                  .payload="dream", .payload_len=5 };
    dream_result_t res3[2];
    int n3 = dreamland_wish(&dl, &w3, res3, 2);
    printf("\nIngenuity wish: found %d\n", n3);
    for (int i=0; i<n3; i++) printf("  -> %s (type=%d, persistent=%d)\n", res3[i].content, res3[i].type, res3[i].persistent);

    wave_index_free(&idx);
    printf("\nDone.\n");
    return 0;
}
