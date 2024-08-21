#pragma once

#include <stdio.h>

/* Percent-encode, see RFC3986 section 2.1. */
void percentencode(FILE *fp, const char *s, size_t len);

/* Escape characters below as HTML 2.0 / XML 1.0. */
void xmlencode(FILE *fp, const char *s, size_t len);

/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
void xmlencodeline(FILE *fp, const char *s, size_t len);
