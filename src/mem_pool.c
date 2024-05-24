#include <assert.h>

#include "mem_pool.h"

void mem_pool_create(struct mem_pool* pool, void* (*create_obj)(void*),
					 size_t capacity, void* user) {
	assert(pool && capacity > 0 && create_obj);
	queue_init(&pool->queue, sizeof(void*), capacity);

	for(size_t k = 0; k < capacity; k++) {
		void* obj = create_obj(user);
		queue_add_blocking(&pool->queue, &obj);
	}
}

void* mem_pool_try_alloc(struct mem_pool* pool) {
	assert(pool);

	void* res;
	return queue_try_remove(&pool->queue, &res) ? res : NULL;
}

void* mem_pool_alloc(struct mem_pool* pool) {
	assert(pool);

	void* res;
	queue_remove_blocking(&pool->queue, &res);
	return res;
}

void mem_pool_free(struct mem_pool* pool, void* obj) {
	assert(pool && obj);
	// cannot fail, because queue always has enough space to store an element
	queue_try_add(&pool->queue, &obj);
}

size_t mem_pool_capacity(struct mem_pool* pool) {
	assert(pool);
	return pool->queue.element_count;
}

bool mem_pool_any_allocated(struct mem_pool* pool) {
	assert(pool);
	return !queue_is_full(&pool->queue);
}