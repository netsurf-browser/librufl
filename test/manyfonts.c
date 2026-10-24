#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rufl.h"

#include "harness.h"
#include "testutils.h"

static char template[] = "/tmp/manyfontsXXXXXX";
static const char *ptmp = NULL;

static int ftw_cb(const char *path, const struct stat *sb,
		int typeflag, struct FTW *ftwbuf)
{
	(void) sb;
	(void) typeflag;
	(void) ftwbuf;

	remove(path);

	return 0;
}

static void cleanup(void)
{
	if (ptmp == NULL)
		return;

	nftw(ptmp, ftw_cb, FOPEN_MAX, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
}

int main(int argc, const char **argv)
{
	char *names[300];
	int x;

	assert(2 == argc);

	ptmp = mkdtemp(template);
	assert(NULL != ptmp);
	atexit(cleanup);
	assert(0 == chdir(ptmp));

	rufl_test_harness_init(380, true, true);

	for (x = 0; x < 300; x++) {
		char buf[64];
		sprintf(buf, "Font%03d", x);
		names[x] = strdup(buf);
		rufl_test_harness_register_font(names[x]);
	}
	rufl_test_harness_set_font_glyph_map(names[0], argv[1]);

	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);
	assert(303 == rufl_family_list_entries);
	assert(NULL != rufl_family_menu);

	rufl_dump_state(true);

	rufl_quit();

	/* Reinit -- should load cache */
	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);
	assert(303 == rufl_family_list_entries);
	assert(NULL != rufl_family_menu);
	/* Done for real this time */
	rufl_quit();

	for (x = 0; x < 300; x++) {
		free(names[x]);
	}

	printf("PASS\n");

	return 0;
}
