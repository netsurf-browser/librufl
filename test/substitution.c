#include <ftw.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rufl.h"

/* dirty! */
#include "../src/rufl_internal.h"

#include "harness.h"
#include "testutils.h"

struct expumap {
	char *fontname;
	size_t num_umaps;
};

struct fontenc {
	char *fontname;
	char *encoding;
	char *filepath;
};

struct cfg {
	const char *datadir;

	struct expumap *expumaps;

	struct fontenc *fontencs;

	size_t n_expumaps;

	size_t n_fontencs;

	int fm_version;
	bool fm_ucs;
	bool preload;
};

static char template[] = "/tmp/substitutionXXXXXX";
static const char *ptmp = NULL;
static struct cfg cfg;

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
	if (cfg.fontencs != NULL ) {
		size_t i;

		for (i = 0; i < cfg.n_fontencs; i++) {
			free(cfg.fontencs[i].encoding);
			free(cfg.fontencs[i].fontname);
			free(cfg.fontencs[i].filepath);
		}
		free(cfg.fontencs);
	}

	if (cfg.expumaps != NULL) {
		size_t i;

		for (i = 0; i < cfg.n_expumaps; i++) {
			free(cfg.expumaps[i].fontname);
		}
		free(cfg.expumaps);
	}

	if (ptmp == NULL)
		return;

	nftw(ptmp, ftw_cb, FOPEN_MAX, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
}

static void parse_expumaps(struct cfg *cfg, char *data, size_t len)
{
	char *p, *s;
	const char *font = NULL, *count = NULL;
	struct expumap *e;
	size_t num_umaps;

	for (p = s = data; p < data+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p) {
				if (font == NULL)
					font = s;
				else if (count == NULL)
					count = s;
			}
			s = p+1;
		}
	}
	if (count == NULL)
		count = s;

	num_umaps = strtoul(count, &p, 10);
	assert((size_t)(p-count) == strlen(count));

	e = realloc(cfg->expumaps, (cfg->n_expumaps + 1) * sizeof(*e));
	assert(e != NULL);

	cfg->expumaps = e;
	cfg->expumaps[cfg->n_expumaps].fontname = strdup(font);
	assert(cfg->expumaps[cfg->n_expumaps].fontname != NULL);
	cfg->expumaps[cfg->n_expumaps].num_umaps = num_umaps;
	cfg->n_expumaps++;
}

static void parse_fontmanager(struct cfg *cfg, char *data, size_t len)
{
	char *p, *s;
	const char *version = NULL, *ucs = NULL, *preload = NULL;

	for (p = s = data; p < data+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p) {
				if (version == NULL)
					version = s;
				else if (ucs == NULL)
					ucs = s;
				else if (preload == NULL)
					preload = s;
			}
			s = p+1;
		}
	}
	if (preload == NULL)
		preload = s;

	cfg->fm_version = strtoul(version, &p, 10);
	assert((size_t)(p-version) == strlen(version));

	cfg->fm_ucs = *ucs == 'U';
	cfg->preload = *preload == 'P';
}

static void parse_directive(struct cfg *cfg, char *linecpy, size_t len)
{
	char *p, *s;
	const char *directive = NULL;

	for (p = s = linecpy; p < linecpy+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p && directive == NULL) {
				directive = s;
				s = p+1;
				break;
			}
			s = p+1;
		}
	}
	if (directive == NULL)
		directive = s;

	if (strcmp("\%expumaps", directive) == 0) {
		parse_expumaps(cfg, s, len - (s - linecpy));
	} else if (strcmp("\%fontmanager", directive) == 0) {
		parse_fontmanager(cfg, s, len - (s - linecpy));
	}
}

static void parse_encoding(struct cfg *cfg, char *linecpy, size_t len)
{
	char *p, *s;
	const char *font = NULL, *encoding = NULL, *file = NULL;
	struct fontenc *fe;

	for (p = s = linecpy; p < linecpy+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p) {
				if (font == NULL)
					font = s;
				else if (encoding == NULL)
					encoding = s;
				else if (file == NULL)
					file = s;
			}
			s = p+1;
		}
	}
	if (file == NULL)
		file = s;

	assert(font != NULL);
	assert(encoding != NULL);
	assert(file != NULL);

	fe = realloc(cfg->fontencs, (cfg->n_fontencs + 1) * sizeof(*fe));
	assert(fe != NULL);

	cfg->fontencs = fe;
	cfg->fontencs[cfg->n_fontencs].fontname = strdup(font);
	assert(cfg->fontencs[cfg->n_fontencs].fontname != NULL);
	cfg->fontencs[cfg->n_fontencs].encoding = strdup(encoding);
	assert(cfg->fontencs[cfg->n_fontencs].encoding != NULL);

	cfg->fontencs[cfg->n_fontencs].filepath = malloc(
			strlen(cfg->datadir) + strlen(file) + 2);
	assert(cfg->fontencs[cfg->n_fontencs].filepath != NULL);
	strcpy(cfg->fontencs[cfg->n_fontencs].filepath, cfg->datadir);
	//XXX: platform-agnostic dirsep?
	cfg->fontencs[cfg->n_fontencs].filepath[strlen(cfg->datadir)] = '/';
	strcpy(cfg->fontencs[cfg->n_fontencs].filepath +
			strlen(cfg->datadir) + 1, file);
	cfg->n_fontencs++;
}

static void line_cb(struct cfg *cfg, const char *line, size_t len)
{
	char *linecpy;

	if (len == 0 || line[0] == '#')
		return;

	linecpy = malloc(len + 1);
	assert(linecpy != NULL);
	memcpy(linecpy, line, len);
	linecpy[len] = '\0';

	if (line[0] == '%')
		parse_directive(cfg, linecpy, len);
	else
		parse_encoding(cfg, linecpy, len);

	free(linecpy);
}

static void parse_cfg(const char *path, struct cfg *cfg,
		void (*cb)(struct cfg *cfg, const char *line, size_t len))
{
	FILE *fp;
	char wbuf[4096];
	size_t nleft = 0;

	fp = fopen(path, "r");
	assert(fp != NULL);

	while (!feof(fp)) {
		char buf[2048];
		size_t nread;
		const char *p, *s;

		nread = fread(buf, 1, sizeof(buf), fp);
		if (nread != sizeof(buf)) {
			assert(ferror(fp) == 0);
		}

		memcpy(wbuf + nleft, buf, nread);
		nleft += nread;

		for (p = s = wbuf; p < wbuf + nleft; p++) {
			if (*p == '\n') {
				cb(cfg, s, p - s);
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
}

static void read_config(const char *path, struct cfg *cfg)
{
	char *pathcpy;

	pathcpy = strdup(path);
	assert(pathcpy != NULL);

	cfg->datadir = dirname(pathcpy);

	parse_cfg(path, cfg, line_cb);

	free(pathcpy);
	cfg->datadir = NULL;
}

int main(int argc, const char **argv)
{
	os_box bbox;
	const int font_size = 160;
	size_t i;

	assert(2 == argc);

	ptmp = mkdtemp(template);
	assert(NULL != ptmp);
	atexit(cleanup);
	assert(0 == chdir(ptmp));

	read_config(argv[1], &cfg);

	rufl_test_harness_init(cfg.fm_version, cfg.fm_ucs, cfg.preload);

	if (!cfg.preload) {
		/* Register fonts and encodings */
		for (i = 0; i < cfg.n_fontencs; i++) {
			rufl_test_harness_register_font(
					cfg.fontencs[i].fontname);
			rufl_test_harness_register_encoding(
					cfg.fontencs[i].encoding);
		}
	}
	/* Configure font glyph mappings */
	for (i = 0; i < cfg.n_fontencs; i++) {
		if (cfg.fm_ucs) {
			/* UCS mode: sparse map */
			rufl_test_harness_set_font_glyph_map(
					cfg.fontencs[i].fontname,
					cfg.fontencs[i].filepath);
		} else {
			/* Legacy mode: encoding file */
			rufl_test_harness_set_font_encoding(
					cfg.fontencs[i].fontname,
					cfg.fontencs[i].encoding,
					cfg.fontencs[i].filepath);
		}
	}

	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);

	/* Measure the bounding box for our desired font size.
	 * We use Corpus as that *must* be present (and the mock
	 * implementation treats all fonts as fixed pitch, so
	 * the result here will suffice for everything). */
	assert(rufl_OK == rufl_font_bbox("Corpus", rufl_WEIGHT_500,
				font_size, &bbox));

	/* Paint a string containing characters from the ASCII, Latin1,
	 * and neither encodings using the ASCII-only font. We expect the
	 * Latin1 characters to be drawn using the Latin1 font. We expect
	 * the unrepresentable characters to result in tofu drawn in Corpus. */
	assert(rufl_OK == rufl_paint("ASCII", rufl_WEIGHT_500, font_size,
			"!\xe2\x80\xa2\xdc\x80", 6, 0, 0, 0));
	if (cfg.fm_ucs) {
		/* UCS mode */
		rufl_test_harness_assert_paint_calls(
		    "ASCII.Medium", "UTF8", font_size, 0, 0, true, false, "!",
		    "Latin1.Medium", "UTF8", font_size, bbox.x1, 0, true, false,
				"\xe2\x80\xa2",
		    /* Tofu: 4 digits, at half size, drawn in a 2x2 grid */
		    "Corpus.Medium", "Latin1", font_size/2,
				2*bbox.x1, bbox.y1/2, false, false, "07",
		    "Corpus.Medium", "Latin1", font_size/2,
				2*bbox.x1, 0, false, false, "00",
		    NULL
		);
	} else {
		/* Legacy mode */
		for (i = 0; i < cfg.n_fontencs; i++) {
			const char *enc = NULL;
			if (strcmp(cfg.fontencs[i].fontname,
						"ASCII.Medium") != 0)
				continue;
			if (strcmp(cfg.fontencs[i].encoding, "Symbol") != 0)
				enc = cfg.fontencs[i].encoding;
			rufl_test_harness_assert_paint_calls(
			    "ASCII.Medium", enc, 160, 0, 0, false, false, "!",
			    "Latin1.Medium", enc, 160, bbox.x1, 0, false, false,
					"\x8f",
			    /* Tofu: 4 digits, at half size,
			     * drawn in a 2x2 grid */
			    "Corpus.Medium", "Latin1", font_size/2,
					2*bbox.x1, bbox.y1/2,
					false, false, "07",
			    "Corpus.Medium", "Latin1", font_size/2,
					2*bbox.x1, 0,
					false, false, "00",

			    NULL
			);
			break;
		}
	}

	rufl_quit();

	printf("PASS\n");

	return 0;
}
