#ifndef MEM_POOL_H
#define MEM_POOL_H

#include "pico/util/queue.h"

/**
 * A thread-safe memory pool.
 */
struct mem_pool {
	queue_t queue;
};

/**
 * Creates a memory pool of given capacity, \p create_obj is called for
 * every object once.
 *
 * @param pool memory pool object
 * @param create_obj function called to initialize every object
 * @param capacity number of object to allocate
 * @param user optional pointer passed to \p create_obj
 */
void mem_pool_create(struct mem_pool* pool, void* (*create_obj)(void*),
					 size_t capacity, void* user);

/**
 * Allocates an object or returns `NULL` if none is available at this moment.
 *
 * @param pool memory pool object
 * @return pointer to an available object
 */
void* mem_pool_try_alloc(struct mem_pool* pool);

/**
 * Blocks the current thread until an object is available.
 *
 * @param pool memory pool object
 * @return pointer to an available object
 */
void* mem_pool_alloc(struct mem_pool* pool);

/**
 * Frees an object owned by this memory pool. Never blocks.
 *
 * @param pool memory pool object
 * @param obj pointer to an object
 */
void mem_pool_free(struct mem_pool* pool, void* obj);

/**
 * Gets the maximum amount of allocated objects possible.
 *
 * @param pool memory pool object
 * @return capacity of the memory pool
 */
size_t mem_pool_capacity(struct mem_pool* pool);

/**
 * Determines whether the pool has any allocated objects.
 *
 * @param pool memory pool object
 * @return whether the pool has any allocated objects
 */
bool mem_pool_any_allocated(struct mem_pool* pool);

#endif