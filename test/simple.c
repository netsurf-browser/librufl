#include <stdio.h>

#include "rufl.h"

#include "testutils.h"

int main(int argc, const char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	assert(rufl_FONT_MANAGER_ERROR == rufl_init());

	printf("PASS\n");

	return 0;
}
