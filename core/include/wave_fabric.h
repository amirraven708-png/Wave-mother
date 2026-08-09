#ifndef WAVE_FABRIC_H
#define WAVE_FABRIC_H
#include <stdint.h>

typedef struct {
    uint64_t task_id;           // شناسه یکتای تسک
    uint64_t required_capacity; // ظرفیت مورد نیاز (CPU/MEM)
    uint64_t rhythm;            // ریتم مقصد
    double   priority;          // اولویت (0.0 = کم, 1.0 = بحرانی)
    uint8_t  transient;         // 1 = سیگنال گذرا (پس از حل شدن حذف شود)
} wave_task_t;

// ارسال تسک به Fabric (درخواست Offload)
int wave_fabric_submit(wave_task_t *task);

// دریافت نتیجه از Fabric (تا timeout مشخص)
int wave_fabric_receive_result(uint64_t task_id, void *buffer, uint32_t timeout_ms);

#endif
