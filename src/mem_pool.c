/*
	Copyright (c) 2024 xtreme8000

	This file is part of picoAVE.

	picoAVE is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	picoAVE is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with picoAVE.  If not, see <http://www.gnu.org/licenses/>.
*/

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