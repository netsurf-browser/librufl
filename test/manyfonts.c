#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rufl.h"

#include "harness.h"
#include "testutils.h"

/* Set to UINT_MAX to ingest them all (warning: takes ~10s) */
#define MAXFONTS 300

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
	char datadir[4096];
	char wbuf[4096];
	FILE *fp;
	char **names = NULL;
	size_t nnames = 0;
	size_t nleft = 0;

	assert(2 == argc);

	ptmp = mkdtemp(template);
	assert(NULL != ptmp);
	atexit(cleanup);
	assert(0 == chdir(ptmp));

	strncpy(datadir, argv[1], sizeof(datadir) - 1);
	datadir[sizeof(datadir)-1] = '\0';

	/* Build path to font catalog */
	dirname(dirname(datadir));
	strcat(datadir, "/FontCat");

	rufl_test_harness_init(380, true, true);

	fp = fopen(datadir, "r");
	assert(NULL != fp);

	while (!feof(fp)) {
		char buf[2048];
		const char *p, *s;
		size_t nread = fread(buf, 1, sizeof(buf), fp);
		if (nread != sizeof(buf)) {
			assert(ferror(fp) == 0);
		}
		memcpy(wbuf + nleft, buf, nread);
		nleft += nread;
		for (p = s = wbuf; p < wbuf + nleft; p++) {
			if (*p == '\n') {
				if (nnames < MAXFONTS) {
					char **tmp = realloc(names,
						(nnames+1) * sizeof(names[0]));
					assert(NULL != tmp);
					names = tmp;
					names[nnames] = malloc(p - s + 1);
					assert(NULL != names[nnames]);
					memcpy(names[nnames], s, p - s);
					names[nnames][p - s] = '\0';
					nnames++;
				}
				s = p+1;
			}
		}
		if (s != wbuf + nleft) {
			memmove(wbuf, s, p - s);
			nleft = p - s;
		} else {
			nleft = 0;
		}
		assert(nleft < sizeof(buf));
	}
	assert(nleft == 0);
	fclose(fp);

	while (nnames < 300) {
		char buf[64];
		char **tmp = realloc(names, (nnames+1) * sizeof(names[0]));
		assert(NULL != tmp);
		names = tmp;
		sprintf(buf, "Font%03zu", nnames);
		names[nnames] = strdup(buf);
		nnames++;
	}

	for (nleft = 0; nleft < nnames; nleft++) {
		rufl_test_harness_register_font(names[nleft]);
	}
	rufl_test_harness_set_font_glyph_map(names[0], argv[1]);

	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);
	assert(3 <= rufl_family_list_entries &&
			rufl_family_list_entries <= nnames + 3);
	assert(NULL != rufl_family_menu);

	rufl_dump_state(true);

	rufl_quit();

	/* Reinit -- should load cache */
	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);
	assert(3 <= rufl_family_list_entries &&
			rufl_family_list_entries <= nnames + 3);
	assert(NULL != rufl_family_menu);
	/* Done for real this time */
	rufl_quit();

	for (nleft = 0; nleft < nnames; nleft++) {
		free(names[nleft]);
	}
	free(names);

	printf("PASS\n");

	return 0;
}
