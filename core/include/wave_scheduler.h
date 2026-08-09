#ifndef WAVE_SCHEDULER_H
#define WAVE_SCHEDULER_H
#include <stdint.h>

typedef struct {
    uint64_t node_id;
    uint64_t available_capacity;   // ظرفیت آزاد (واحد دلخواه)
    double   reputation;           // شهرت (0-100)
    double   phase;                // فاز کنونی نود (درجه)
    double   latency_ms;           // تأخیر شبکه به میلی‌ثانیه
} node_candidate_t;

/**
 * انتخاب بهترین نود برای Offload بر اساس فرمول:
 * Score = (Potential_Norm * 0.4) + (Reputation_Norm * 0.3) + (Phase_Alignment * 0.3)
 * 
 * @param candidates    آرایه‌ای از نودهای داوطلب
 * @param count         تعداد نودها
 * @param local_phase   فاز محلی نود درخواست‌کننده (برای محاسبهٔ هم‌ترازی فاز)
 * @return              ایندکس نود انتخاب‌شده در آرایه، یا -1 در صورت خطا
 */
int wave_scheduler_select(node_candidate_t *candidates, int count, double local_phase);

#endif
