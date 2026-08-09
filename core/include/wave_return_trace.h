#ifndef WAVE_RETURN_TRACE_H
#define WAVE_RETURN_TRACE_H
#include <stdint.h>

typedef struct {
    uint64_t task_id;          // شناسهٔ تسک (همان که در Offload ثبت شد)
    uint64_t executor_id;      // شناسهٔ نود اجراکننده
    uint64_t result_hash;      // هش خروجی (برای تطابق)
    uint32_t exit_code;        // 0 = موفقیت
    uint32_t data_size;        // اندازهٔ دادهٔ خروجی
    uint8_t  data[1024];       // اولین ۱KB خروجی
} return_trace_t;

return_trace_t* trace_create(uint64_t task_id, uint64_t executor_id,
                             const void *data, uint32_t data_size, uint32_t exit_code);
void trace_free(return_trace_t *t);
#endif
