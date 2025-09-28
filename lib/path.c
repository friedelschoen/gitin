#include "path.h"

#include <string.h>

void pathunhide(char* path) {
	for (char* chr = path; *chr; chr++) {
		if (*chr == '.' && (chr == path || chr[-1] == '/') && chr[1] != '/')
			*chr = '-';
	}
}
