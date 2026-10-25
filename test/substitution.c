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

struct expectation {
	char *fontname;  /**< Fully qualified font name */
	char *encoding;  /**< Expected encoding */
	char *text;  /**< Expected text (in encoding) */
	int xoff;  /**< x offset in _characters_ */
	/**< y divisor_
	 * Text is ordinarily placed on a common baseline
	 * and this value will be *zero*. However, tofu
	 * uses two rows of half-height characters and
	 * this value will be 2 for the top row, whose
	 * baseline is half way up the normal row height.
	 */
	int ydiv;
	int sdiv;  /**< size divisor */
};

struct testcase {
	char *fontname;  /**< Font to request */
	char *input;  /**< UTF-8 encoded input */
	struct expectation *expect;
	size_t n_expect;
	rufl_style style;  /**< rufl_paint style */
};

struct cfg {
	const char *datadir;

	struct expumap *expumaps;
	struct fontenc *fontencs;
	struct testcase *testcases;

	size_t n_expumaps;
	size_t n_fontencs;
	size_t n_testcases;

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
	if (cfg.testcases != NULL) {
		size_t i, j;

		for (i = 0; i < cfg.n_testcases; i++) {
			for (j = 0; j < cfg.testcases[i].n_expect; j++) {
				free(cfg.testcases[i].expect[j].fontname);
				free(cfg.testcases[i].expect[j].encoding);
				free(cfg.testcases[i].expect[j].text);
			}
			free(cfg.testcases[i].expect);
			free(cfg.testcases[i].input);
			free(cfg.testcases[i].fontname);
		}
		free(cfg.testcases);
	}
	if (cfg.fontencs != NULL) {
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

static int fromhex(char val)
{
	if ('0' <= val && val <= '9')
		return val - '0';
	else if ('A' <= val && val <= 'F')
		return val - 'A' + 10;
	else if ('a' <= val && val <= 'f')
		return val - 'a' + 10;
	assert(0);
	return -1;
}

static char *unescape(char *p) {
	enum { normal, backslash, esc0, esc1 } state = normal;
	char *s = p;
	char *outp = p;
	char c;

	/* Modify the data in place. It will either remain the
	 * same size, or get smaller (depending on the presence
	 * of escape sequences); it can never grow. */
	while (*p != '\0') {
		if (state == normal && *p == '\\') {
			state = backslash;
		} else if (state == normal) {
			*(outp++) = *p;
		} else if (state == backslash && *p == 'x') {
			c = 0;
			state = esc0;
		} else if (state == backslash) {
			*(outp++) = '\\';
			state = normal;
		} else if (state == esc0) {
			c = fromhex(*p);
			state = esc1;
		} else if (state == esc1) {
			c = (((uint8_t) c) << 4) | fromhex(*p);
			*(outp++) = c;
			state = normal;
		}
		p++;
	}
	*outp = '\0';
	return s;
}

static rufl_style style_from_string(const char *s)
{
	char *end;
	uint32_t val = strtoul(s, &end, 10);

	assert(100 <= val && val <= 900 && val % 100 == 0);

	return (rufl_style) ((val / 100) | (*end == '/' ? rufl_SLANTED : 0));
}

static void parse_testcase(struct cfg *cfg, char *data, size_t len)
{
	struct testcase *tc;
	char *p, *s, *input = NULL;
	const char *index = NULL, *font = NULL, *style = NULL;
	uint32_t testnum;

	for (p = s = data; p < data+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p) {
				if (index == NULL)
					index = s;
				else if (font == NULL)
					font = s;
				else if (style == NULL)
					style = s;
				else if (input == NULL)
					input = s;
			}
			s = p+1;
		}
	}
	if (input == NULL)
		input = s;

	assert(index != NULL);
	assert(font != NULL);
	assert(style != NULL);
	assert(input != NULL);

	testnum = strtoul(index, &p, 10);
	assert((size_t)(p-index) == strlen(index));

	/* Testcases must be contiguous and monotonic and start from 1 */
	assert(testnum == cfg->n_testcases + 1);

	tc = realloc(cfg->testcases, (cfg->n_testcases + 1) * sizeof(*tc));
	assert(tc != NULL);

	cfg->testcases = tc;
	cfg->testcases[cfg->n_testcases].fontname = strdup(font);
	assert(cfg->testcases[cfg->n_testcases].fontname != NULL);
	cfg->testcases[cfg->n_testcases].input = strdup(unescape(input));
	assert(cfg->testcases[cfg->n_testcases].input != NULL);
	cfg->testcases[cfg->n_testcases].expect = NULL;
	cfg->testcases[cfg->n_testcases].n_expect = 0;
	cfg->testcases[cfg->n_testcases].style = style_from_string(style);
	cfg->n_testcases++;
}

static void parse_expect(struct cfg *cfg, char *data, size_t len)
{
	struct expectation *exp;
	char *p, *s, *text = NULL;
	const char *index = NULL, *fontname = NULL, *encoding = NULL;
	const char *xoff = NULL, *ydiv = NULL, *sdiv = NULL;
	uint32_t testnum;

	for (p = s = data; p < data+len; p++) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			if (s != p) {
				if (index == NULL)
					index = s;
				else if (fontname == NULL)
					fontname = s;
				else if (encoding == NULL)
					encoding = s;
				else if (xoff == NULL)
					xoff = s;
				else if (ydiv == NULL)
					ydiv = s;
				else if (sdiv == NULL)
					sdiv = s;
				else if (text == NULL)
					text = s;
			}
			s = p+1;
		}
	}
	if (text == NULL)
		text = s;

	assert(index != NULL);
	assert(fontname != NULL);
	assert(encoding != NULL);
	assert(xoff != NULL);
	assert(ydiv != NULL);
	assert(sdiv != NULL);
	assert(text != NULL);

	testnum = strtoul(index, &p, 10);
	assert((size_t)(p-index) == strlen(index));

	/* Testcases must be defined before their expectations */
	assert(testnum <= cfg->n_testcases);

	exp = realloc(cfg->testcases[testnum-1].expect,
			(cfg->testcases[testnum-1].n_expect + 1) *
			sizeof(*exp));
	assert(exp != NULL);

	cfg->testcases[testnum-1].expect = exp;
	exp[cfg->testcases[testnum-1].n_expect].fontname = strdup(fontname);
	assert(exp[cfg->testcases[testnum-1].n_expect].fontname != NULL);
	exp[cfg->testcases[testnum-1].n_expect].encoding = strdup(encoding);
	assert(exp[cfg->testcases[testnum-1].n_expect].encoding != NULL);
	exp[cfg->testcases[testnum-1].n_expect].text = strdup(unescape(text));
	assert(exp[cfg->testcases[testnum-1].n_expect].text != NULL);
	exp[cfg->testcases[testnum-1].n_expect].xoff = strtoul(xoff, &p, 10);
	assert((size_t)(p-xoff) == strlen(xoff));
	exp[cfg->testcases[testnum-1].n_expect].ydiv = strtoul(ydiv, &p, 10);
	assert((size_t)(p-ydiv) == strlen(ydiv));
	exp[cfg->testcases[testnum-1].n_expect].sdiv = strtoul(sdiv, &p, 10);
	assert((size_t)(p-sdiv) == strlen(sdiv));
	cfg->testcases[testnum-1].n_expect++;
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
	} else if (strcmp("\%tcase", directive) == 0) {
		parse_testcase(cfg, s, len - (s - linecpy));
	} else if (strcmp("\%expect", directive) == 0) {
		parse_expect(cfg, s, len - (s - linecpy));
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

	/* Run the test cases */
	for (i = 0; i < cfg.n_testcases; i++) {
		const rufl_test_harness_paint_call *call = NULL;
		const struct testcase *tc = &cfg.testcases[i];
		size_t e;

		rufl_test_harness_discard_paint_calls();

		assert(rufl_OK == rufl_paint(tc->fontname, tc->style,
			font_size, tc->input, strlen(tc->input), 0, 0, 0));

		for (e = 0; e < tc->n_expect; e++) {
			rufl_test_harness_paint_call ecall;
			const struct expectation *exp = &tc->expect[e];

			ecall.font.name = exp->fontname;
			if (strcmp(exp->encoding, "Symbol") == 0)
				ecall.font.encoding = NULL;
			else
				ecall.font.encoding = exp->encoding;
			/* XXX: collusion with mock! */
			ecall.font.xres = ecall.font.yres = 90;
			ecall.font.xsize = font_size / exp->sdiv;
			ecall.font.ysize = font_size / exp->sdiv;
			ecall.xpos = exp->xoff * bbox.x1;
			ecall.ypos = 0;
			if (exp->ydiv != 0) {
				ecall.ypos += bbox.y1 / exp->ydiv;
			}
			ecall.ucs4 = strcmp(exp->encoding, "UTF8") == 0;
			ecall.trfm = false;
			ecall.len = strlen(exp->text);

			call = rufl_test_harness_paint_enumerate(call);
			rufl_test_harness_assert_paint_call(call, &ecall,
					exp->text);
		}

		call = rufl_test_harness_paint_enumerate(call);
		assert(call == NULL);
	}

	rufl_quit();

	printf("PASS\n");

	return 0;
}
