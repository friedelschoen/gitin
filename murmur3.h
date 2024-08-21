#pragma once

#include <stdint.h>

uint32_t murmurhash3(const void *key, int len, uint32_t seed);
