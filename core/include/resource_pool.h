#ifndef RESOURCE_POOL_H
#define RESOURCE_POOL_H
#include <stdint.h>
typedef struct {
    uint64_t allocated_resources;
    uint64_t available_units;
} resource_pool_t;
void resource_pool_init(resource_pool_t *pool, uint64_t initial_units);
uint64_t resource_pool_contribute(resource_pool_t *pool, uint64_t units);
uint64_t resource_pool_consume(resource_pool_t *pool, uint64_t units);
#endif
