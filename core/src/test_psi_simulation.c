#include <stdio.h>
#include "wave_psi_core.h"

int main() {
    psi_core_engine_t psi;
    psi_core_init(&psi);

    double residuals[] = {84.5, 62.1, 45.8, 12.4, 2.1, 0.05};
    double t = 0.0;
    printf("=== PSI-CORE: Geometric Breathing Simulation ===\n");
    for (int i = 0; i < 6; i++) {
        psi_core_breathe(&psi, residuals[i], 0.1);
        t += 0.1;
        printf("t=%.2fs | Residual: %.2f | Dim: %.2f | Action: %s\n",
               t, residuals[i], psi.current_dim,
               (residuals[i] > 10.0) ? "Inhaling (Expanding)" :
               (residuals[i] > 0.1) ? "Exhaling (Contracting)" : "HARMONY ACHIEVED");
    }
    if (psi_core_extract_harmony(&psi) > 0.5)
        printf("\nThe fabric is alive and in resonance.\n");
    return 0;
}
