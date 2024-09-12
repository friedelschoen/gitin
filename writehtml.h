#include "writerepo.h"

#include <stdio.h>

void writeheader(FILE* fp, const struct repoinfo* info, const char* relpath, const char* name, const char* description);
void writefooter(FILE* fp);
