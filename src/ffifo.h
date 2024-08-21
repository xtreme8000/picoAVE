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

#ifndef FFIFO_H
#define FFIFO_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pico/sync.h"
#include "utils.h"

#ifndef CUSTOM_FUNC
#define CUSTOM_FUNC(a, ...) a##__VA_ARGS__
#endif

#define FIFO_DEF(name, type)                                                   \
	struct name {                                                              \
		size_t read, write;                                                    \
		size_t mask;                                                           \
		type* data;                                                            \
	};                                                                         \
                                                                               \
	static inline void CUSTOM_FUNC(name, _init)(struct name * f,               \
												size_t min_length) {           \
		assert(f);                                                             \
		static_assert(sizeof(min_length) == sizeof(uint32_t));                 \
		uint32_t length = next_pow2(min_length);                               \
		f->read = f->write = 0;                                                \
		f->mask = length - 1;                                                  \
		f->data = malloc(sizeof(type) * length);                               \
	}                                                                          \
                                                                               \
	static inline void CUSTOM_FUNC(name, _destroy)(struct name * f) {          \
		assert(f && f->data);                                                  \
		free(f->data);                                                         \
		f->data = NULL;                                                        \
	}                                                                          \
                                                                               \
	static inline bool CUSTOM_FUNC(name, _empty)(struct name * f) {            \
		assert(f);                                                             \
		return f->write == f->read;                                            \
	}                                                                          \
                                                                               \
	static inline bool CUSTOM_FUNC(name, _full)(struct name * f) {             \
		assert(f);                                                             \
		return ((f->write + 1) & f->mask) == f->read;                          \
	}                                                                          \
                                                                               \
	static inline bool CUSTOM_FUNC(name, _push)(struct name * f,               \
												type * data) {                 \
		assert(f&& data);                                                      \
                                                                               \
		if(CUSTOM_FUNC(name, _full)(f))                                        \
			return false;                                                      \
                                                                               \
		f->data[f->write] = *data;                                             \
		f->write = (f->write + 1) & f->mask;                                   \
                                                                               \
		return true;                                                           \
	}                                                                          \
                                                                               \
	static inline bool CUSTOM_FUNC(name, _pop)(struct name * f, type * data) { \
		assert(f&& data);                                                      \
                                                                               \
		if(CUSTOM_FUNC(name, _empty)(f))                                       \
			return false;                                                      \
                                                                               \
		*data = f->data[f->read];                                              \
		f->read = (f->read + 1) & f->mask;                                     \
                                                                               \
		return true;                                                           \
	}                                                                          \
                                                                               \
	static inline bool CUSTOM_FUNC(name, _peek)(struct name * f,               \
												type * data) {                 \
		assert(f&& data);                                                      \
                                                                               \
		if(CUSTOM_FUNC(name, _empty)(f))                                       \
			return false;                                                      \
                                                                               \
		*data = f->data[f->read];                                              \
                                                                               \
		return true;                                                           \
	}                                                                          \
                                                                               \
	static inline void CUSTOM_FUNC(name, _push_blocking)(struct name * f,      \
														 type * data) {        \
		assert(f&& data);                                                      \
                                                                               \
		while(1) {                                                             \
			uint32_t mask = save_and_disable_interrupts();                     \
                                                                               \
			if(CUSTOM_FUNC(name, _push)(f, data)) {                            \
				restore_interrupts(mask);                                      \
				break;                                                         \
			}                                                                  \
                                                                               \
			restore_interrupts(mask);                                          \
			__wfi();                                                           \
		}                                                                      \
	}                                                                          \
                                                                               \
	static inline void CUSTOM_FUNC(name, _pop_blocking)(struct name * f,       \
														type * data) {         \
		assert(f&& data);                                                      \
                                                                               \
		while(1) {                                                             \
			uint32_t mask = save_and_disable_interrupts();                     \
                                                                               \
			if(CUSTOM_FUNC(name, _pop)(f, data)) {                             \
				restore_interrupts(mask);                                      \
				break;                                                         \
			}                                                                  \
                                                                               \
			restore_interrupts(mask);                                          \
			__wfi();                                                           \
		}                                                                      \
	}

#endif