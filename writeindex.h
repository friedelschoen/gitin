#include <stdio.h>

#include "writerepo.h"

void writeheader_index(FILE *fp, const char* description);
void writefooter_index(FILE *fp);
int writelog_index(FILE *fp, const struct repoinfo *info);
