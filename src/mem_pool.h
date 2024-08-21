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