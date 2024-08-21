#include "deltainfo.h"

#include <memory.h>

void
deltainfo_free(struct deltainfo *di)
{
	if (!di)
		return;
	git_patch_free(di->patch);
	memset(di, 0, sizeof(*di));
	free(di);
}
