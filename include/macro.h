#pragma once

#define FORMAT(a, b) __attribute__((format(printf, a, b)))
#define LEN(s)       (sizeof(s) / sizeof(*s))
#define FORMASK(var, in)                      \
	for (int var = 1; var <= (in); var <<= 1) \
		if (var & (in))
