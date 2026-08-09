#ifndef FEEDBACK_CALIBRATOR_H
#define FEEDBACK_CALIBRATOR_H
#include <stdint.h>
typedef struct {
    double total_potential;
    double network_activity;
    double system_stability;
    double calibrated_amplitude;
    double target_frequency;
} calibrator_state_t;
void calibrator_init(calibrator_state_t *cal);
void calibrator_update(calibrator_state_t *cal, uint64_t total_capacity, uint64_t active_deficits);
#endif
