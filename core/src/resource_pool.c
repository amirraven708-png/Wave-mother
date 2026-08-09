#include "resource_pool.h"
void resource_pool_init(resource_pool_t *pool, uint64_t initial_units) {
    if (!pool) return;
    pool->available_units = initial_units;
    pool->allocated_resources = 0;
}
uint64_t resource_pool_contribute(resource_pool_t *pool, uint64_t units) {
    if (!pool) return 0;
    pool->available_units += units;
    return pool->available_units;
}
uint64_t resource_pool_consume(resource_pool_t *pool, uint64_t units) {
    if (!pool) return 0;
    if (pool->available_units < units) {
        uint64_t taken = pool->available_units;
        pool->available_units = 0;
        pool->allocated_resources += taken;
        return taken;
    }
    pool->available_units -= units;
    pool->allocated_resources += units;
    return units;
}
