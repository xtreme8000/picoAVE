#include "str_builder.h"

char* str_append(char* s, char* a) {
	while(*a)
		*(s++) = *(a++);

	return s;
}

char* str_uint(char* s, uint32_t i) {
	char buf[10];
	char* last = buf;

	while(1) {
		*(last++) = (i % 10) + '0';
		i /= 10;

		if(!i)
			break;
	}

	while(buf != last)
		*(s++) = *(--last);

	return s;
}