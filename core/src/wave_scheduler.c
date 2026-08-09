#include <math.h>
#include "wave_scheduler.h"

// نرمال‌سازی ظرفیت: هر ظرفیت بالای 1000 را 1.0 در نظر می‌گیرد
static double normalize_capacity(uint64_t cap) {
    if (cap >= 1000) return 1.0;
    return (double)cap / 1000.0;
}

// نرمال‌سازی شهرت: مقدار بین 0 تا 1
static double normalize_reputation(double rep) {
    if (rep >= 100.0) return 1.0;
    if (rep <= 0.0)   return 0.0;
    return rep / 100.0;
}

// محاسبهٔ هم‌ترازی فاز: هرچه اختلاف فاز کمتر باشد، امتیاز بالاتر است
// اختلاف 0 درجه → امتیاز 1.0
// اختلاف 180 درجه → امتیاز 0.0
static double phase_alignment(double local, double remote) {
    double diff = fabs(local - remote);
    if (diff > 180.0) diff = 360.0 - diff;
    return 1.0 - (diff / 180.0);
}

int wave_scheduler_select(node_candidate_t *candidates, int count, double local_phase) {
    if (!candidates || count <= 0) return -1;
    int best_idx = -1;
    double best_score = -1.0;

    for (int i = 0; i < count; i++) {
        double cap_norm = normalize_capacity(candidates[i].available_capacity);
        double rep_norm = normalize_reputation(candidates[i].reputation);
        double ph_align = phase_alignment(local_phase, candidates[i].phase);

        // امتیاز نهایی با وزن‌های مشخص
        double score = (cap_norm * 0.4) + (rep_norm * 0.3) + (ph_align * 0.3);

        // جریمهٔ تأخیر: اگر latency_ms بالا باشد، امتیاز کاهش می‌یابد
        if (candidates[i].latency_ms > 100.0) {
            score *= 0.5;  // نصف کردن امتیاز برای تأخیر بالای 100ms
        }

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    return best_idx;
}
