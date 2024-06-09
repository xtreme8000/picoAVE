#ifndef STR_BUILDER_H
#define STR_BUILDER_H

#include <stdint.h>

/**
 * Finish the string. Appends a zero character.
 */
static inline void str_finish(char* s) {
	*s = 0;
}

/**
 * Append a single character to the string.
 */
static inline char* str_char(char* s, char c) {
	*(s++) = c;
	return s;
}

/**
 * Append another string to the string.
 */
char* str_append(char* s, char* a);

/**
 * Append an unsigned int to the string.
 */
char* str_uint(char* s, uint32_t i);

#endif