#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dreamland.h"

static double phase_diff_rad(double a, double b) {
    double d = fabs(a - b);
    if (d > M_PI) d = 2.0 * M_PI - d;
    return d;
}

int dreamland_find_resonance(dreamland_t *dl, const wish_t *wish,
                             resonance_space_t *spaces, int max_spaces) {
    if (!dl->traces) return 0;
    int found = 0;
    for (size_t i = 0; i < dl->trace_count && found < max_spaces; i++) {
        double tp = fmod(dl->traces[i].phase, 10000.0) / 10000.0 * 2.0 * M_PI;
        double diff = phase_diff_rad(tp, wish->phase);
        if (diff < wish->phase_tolerance * 2.0) {
            spaces[found].source_rhythm = dl->traces[i].rhythm;
            spaces[found].resonance_strength = 1.0 - diff / (wish->phase_tolerance * 2.0);
            spaces[found].phase_alignment = 1.0 - diff / M_PI;
            spaces[found].is_temporary = 1;
            strncpy(spaces[found].combined_content, dl->traces[i].content,
                    sizeof(spaces[found].combined_content)-1);
            found++;
        }
    }
    return found;
}

int dreamland_combine_temporary(const resonance_space_t *spaces, int space_count,
                                char *output, int max_output_len) {
    output[0] = '\0';
    int remaining = max_output_len;
    for (int i = 0; i < space_count && remaining > 10; i++) {
        int len = strlen(spaces[i].combined_content);
        if (len > remaining - 5) len = remaining - 5;
        strncat(output, spaces[i].combined_content, len);
        if (i < space_count - 1) strncat(output, " + ", 3);
        remaining = max_output_len - strlen(output);
    }
    return strlen(output);
}
