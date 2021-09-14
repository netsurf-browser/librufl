#include <stdio.h>

#include "rufl.h"

#include "harness.h"
#include "testutils.h"

int main(int argc, const char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	rufl_test_harness_init(380, true, true);

	assert(rufl_OK == rufl_init());
	rufl_quit();

	printf("PASS\n");

	return 0;
}
