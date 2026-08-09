#include "feedback_calibrator.h"
void calibrator_init(calibrator_state_t *cal) {
    if (!cal) return;
    cal->total_potential = 1000.0;
    cal->network_activity = 1.0;
    cal->system_stability = 100.0;
    cal->calibrated_amplitude = 1.0;
    cal->target_frequency = 10.0;
}
void calibrator_update(calibrator_state_t *cal, uint64_t total_capacity, uint64_t active_deficits) {
    if (!cal) return;
    cal->total_potential = (double)total_capacity;
    if (active_deficits > 0) {
        cal->calibrated_amplitude = 1.0 + ((double)active_deficits / (total_capacity + 1.0));
        cal->system_stability -= 0.1;
    } else {
        cal->calibrated_amplitude = cal->calibrated_amplitude * 0.95 + 1.0 * 0.05;
        cal->system_stability = cal->system_stability * 0.98 + 100.0 * 0.02;
    }
}
