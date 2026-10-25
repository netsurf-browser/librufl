#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "harness-priv.h"

rufl_test_harness_t *h = NULL;

static void rufl_test_harness_free(void)
{
	size_t ni, ei;

	rufl_test_harness_discard_paint_calls();
	if (h->glyph_maps != NULL) {
		for (ni = 0; ni != h->n_font_names; ni++) {
			free(h->glyph_maps[ni]);
		}
	}
	free(h->glyph_maps);
	free(h->font_names);
	free(h->encodings);
	if (h->encoding_filenames != NULL) {
		for (ni = 0; ni != h->n_font_names; ni++) {
			for (ei = 0; ei != h->n_encodings + 1; ei++) {
				free(h->encoding_filenames[
						(ni * (h->n_encodings + 1)) + ei]);
			}
		}
	}
	free(h->encoding_filenames);
	free(h);
}

void rufl_test_harness_init(int fm_version, bool fm_ucs, bool preload)
{
	h = calloc(1, sizeof(*h));
	assert(h != NULL);

	h->fm_version = fm_version;
	h->fm_ucs = fm_ucs;
	h->fm_broken_fec = fm_version < 364;

	if (preload) {
		/* Register ROM fonts as a convenience */
		rufl_test_harness_register_font("Corpus.Bold");
		rufl_test_harness_register_font("Corpus.Bold.Oblique");
		rufl_test_harness_register_font("Corpus.Medium");
		rufl_test_harness_register_font("Corpus.Medium.Oblique");
		rufl_test_harness_register_font("Homerton.Bold");
		rufl_test_harness_register_font("Homerton.Bold.Oblique");
		rufl_test_harness_register_font("Homerton.Medium");
		rufl_test_harness_register_font("Homerton.Medium.Oblique");
		rufl_test_harness_register_font("Trinity.Bold");
		rufl_test_harness_register_font("Trinity.Bold.Italic");
		rufl_test_harness_register_font("Trinity.Medium");
		rufl_test_harness_register_font("Trinity.Medium.Italic");

		/* Register encodings as a convenience */
		rufl_test_harness_register_encoding("Cyrillic");
		rufl_test_harness_register_encoding("Greek");
		rufl_test_harness_register_encoding("Hebrew");
		rufl_test_harness_register_encoding("Latin1");
		rufl_test_harness_register_encoding("Latin2");
		rufl_test_harness_register_encoding("Latin3");
		rufl_test_harness_register_encoding("Latin4");
		rufl_test_harness_register_encoding("Latin5");
		rufl_test_harness_register_encoding("Latin6");
		rufl_test_harness_register_encoding("Latin7");
		rufl_test_harness_register_encoding("Latin8");
		rufl_test_harness_register_encoding("Latin9");
		rufl_test_harness_register_encoding("Latin10");
		if (fm_ucs)
			rufl_test_harness_register_encoding("UTF8");
		rufl_test_harness_register_encoding("Welsh");
	}

	atexit(rufl_test_harness_free);
}

void rufl_test_harness_register_font(const char *name)
{
	const char **names;

	/* Encoding paths must be registered last */
	assert(h->encoding_filenames == NULL);

	names = realloc(h->font_names,
			(h->n_font_names + 1) * sizeof(*names));
	assert(names != NULL);

	h->font_names = names;

	h->font_names[h->n_font_names++] = name;
}

void rufl_test_harness_register_encoding(const char *encoding)
{
	const char **encodings;

	/* Encoding paths must be registered last */
	assert(h->encoding_filenames == NULL);

	encodings = realloc(h->encodings,
			(h->n_encodings + 1) * sizeof(*encodings));
	assert(encodings != NULL);

	h->encodings = encodings;

	h->encodings[h->n_encodings++] = encoding;
}

void rufl_test_harness_set_font_encoding(const char *fontname,
		const char *encoding, const char *path)
{
	size_t ni, ei;

	if (h->encoding_filenames == NULL) {
		h->encoding_filenames = calloc(
				h->n_font_names * (h->n_encodings + 1),
				sizeof(*h->encoding_filenames));
		assert(h->encoding_filenames != NULL);
	}

	/* Find font index */
	for (ni = 0; ni < h->n_font_names; ni++) {
		if (strcmp(h->font_names[ni], fontname) == 0)
			break;
	}
	assert(ni != h->n_font_names);

	/* Find encoding index */
	if (strcmp("Symbol", encoding) == 0) {
		ei = h->n_encodings;
	} else {
		for (ei = 0; ei < h->n_encodings; ei++) {
			if (strcmp(h->encodings[ei], encoding) == 0)
				break;
		}
		assert(ei != h->n_encodings);
	}

	if (h->encoding_filenames[(ni * (h->n_encodings + 1)) + ei] != NULL)
		free(h->encoding_filenames[(ni * (h->n_encodings + 1)) + ei]);
	h->encoding_filenames[(ni * (h->n_encodings + 1)) + ei] = strdup(path);
	assert(h->encoding_filenames[(ni * (h->n_encodings + 1)) + ei] != NULL);
}


#define GM_CHUNK 256
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

static rufl_test_harness_glyph_map *parse_gm_line(
		rufl_test_harness_glyph_map *map, const char *line,
		size_t len)
{
	const char *p;
	size_t chunks;
	int index = 0, num = 1, codepoint = 0;

	/* The glyph map is a sparse encoding from glyph index -> codepoint
	 * ordered by glyph index and with contiguous range support for
	 * simple inputs:
	 *
	 *   0001:0020    # Single glyph
	 *   0002+8:0022  # 8 contiguous glyphs at indices 2-9; U+0022-U+0029
	 *   !:0030       # U+0030 is explicitly unmapped
	 */

	if (len == 0)
		return map;

	if (*line == '!') {
		index = -1;
		for (p = line + 2; p < line + len; p++) {
			codepoint = (codepoint << 4) | fromhex(*p);
		}
	} else {
		for (p = line; p < line + len; p++) {
			if (*p == ':') {
				assert(codepoint != 0);
				if (index == 0) {
					index = codepoint;
				} else {
					num = codepoint;
				}
				codepoint = 0;
			} else if (*p == '+') {
				assert(codepoint != 0);
				index = codepoint;
				codepoint = 0;
			} else {
				codepoint = (codepoint << 4) | fromhex(*p);
			}
		}
	}

	chunks = (map->n_entries + num + GM_CHUNK - 1) & ~(GM_CHUNK - 1);
	if (chunks != ((map->n_entries + GM_CHUNK - 1) & ~(GM_CHUNK - 1))) {
		rufl_test_harness_glyph_map *tmp;
		tmp = realloc(map, sizeof(*map) +
				(chunks + GM_CHUNK) *
				sizeof(rufl_test_harness_glyph_map_entry));
		assert(tmp != NULL);
		map = tmp;
	}

	while (num-- > 0) {
		assert(0 < codepoint && codepoint <= 0x10ffff);
		map->entries[map->n_entries].codepoint = codepoint;
		map->entries[map->n_entries].index = index;
		map->n_entries += 1;
		index += 1;
		codepoint += 1;
	}

	return map;
}

static int glyph_map_cmp(const void *z1, const void *z2)
{
	const rufl_test_harness_glyph_map_entry *entry1 = z1;
	const rufl_test_harness_glyph_map_entry *entry2 = z2;
	if (entry1->codepoint < entry2->codepoint)
		return -1;
	else if (entry2->codepoint < entry1->codepoint)
		return 1;
	return 0;
}

void rufl_test_harness_set_font_glyph_map(const char *fontname,
		const char *path)
{
	rufl_test_harness_glyph_map *map;
	FILE *fp;
	char *buf;
	const char *p, *end, *l;
	size_t len, read;
	size_t ni;

	if (h->glyph_maps == NULL) {
		h->glyph_maps = calloc(h->n_font_names, sizeof(*h->glyph_maps));
		assert(h->glyph_maps != NULL);
	}

	map = calloc(1, sizeof(*map) +
			GM_CHUNK * sizeof(rufl_test_harness_glyph_map_entry));
	assert(map != NULL);

	/* Find font index */
	for (ni = 0; ni < h->n_font_names; ni++) {
		if (strcmp(h->font_names[ni], fontname) == 0)
			break;
	}
	assert(ni != h->n_font_names);

	fp = fopen(path, "r");
	assert(fp != NULL);

	fseek(fp, 0, SEEK_END);
	len = (size_t) ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = malloc(len);
	assert(buf != NULL);

	read = fread(buf, 1, len, fp);
	assert(read == len);

	fclose(fp);

	p = l = buf;
	end = buf + read;
	while (p < end) {
		if (*p == '\n') {
			map = parse_gm_line(map, l, p-l);
			l = p+1;
		}
		p++;
	}

	free(buf);

	/* Sort the glyph map by codepoint */
	qsort(map->entries, map->n_entries, sizeof(map->entries[0]),
				glyph_map_cmp);


	h->glyph_maps[ni] = map;
}
#undef GM_CHUNK

const rufl_test_harness_paint_call *rufl_test_harness_paint_enumerate(
		const rufl_test_harness_paint_call *current)
{
	rufl_test_harness_paint_call_internal *icall;

	if (h->paint_calls == NULL)
		return NULL;

	if (current == NULL) {
		icall = h->paint_calls;
	} else {
		icall = (rufl_test_harness_paint_call_internal *) (
				((const uint8_t *)((const void *) current)) -
				offsetof(rufl_test_harness_paint_call_internal,
				pub));
		icall = icall->next;
	}

	if (icall == NULL)
		return NULL;

	icall->pub.font.name = h->font_names[icall->name];
	/* Symbol fonts have no encoding */
	if (icall->encoding < h->n_encodings) {
		icall->pub.font.encoding = h->encodings[icall->encoding];
	} else {
		icall->pub.font.encoding = NULL;
	}
	return &icall->pub;
}

void rufl_test_harness_assert_paint_call(
		const rufl_test_harness_paint_call *call,
		const rufl_test_harness_paint_call *exp,
		const char *text)
{
	uint32_t ucs4 = 0;
	char *buf, *outp, *p, *end;

	assert(call != NULL);
	assert(exp != NULL);
	assert(text != NULL);

	/* First, assert that all the non-text parameters match */
	assert(call->trfm == exp->trfm);
	assert(call->ucs4 == exp->ucs4);
	assert(call->xpos == exp->xpos);
	assert(call->ypos == exp->ypos);
	assert(call->font.xsize == exp->font.xsize);
	assert(call->font.ysize == exp->font.ysize);
	assert(call->font.xres == exp->font.xres);
	assert(call->font.yres == exp->font.yres);
	assert(strcmp(call->font.name, exp->font.name) == 0);
	assert((call->font.encoding == NULL && exp->font.encoding == NULL) ||
			strcmp(call->font.encoding, exp->font.encoding) == 0);

	/* Now match the text data */
	if (call->ucs4 == false) {
		/* Simple case: everything is 8 bit */
		assert(call->len == exp->len);
		assert(memcmp(call->data, text, exp->len) == 0);
		return;
	}

	/* UCS4: convert to UTF-8 so that string constants can be used
	 * without difficulty. As the conversion is deterministic, it's
	 * semantically equivalent. */
	p = end = outp = buf = malloc(call->len + 1);
	assert(buf != NULL);

	/* Read the characters backwards and build the UTF-8 sequences
	 * backwards, resulting in a reversed UTF-8 byte stream. */
	for (size_t i = call->len; i > 0; i--) {
		const char *s = p;

		/* Endian agnostic read: result is always big endian */
		ucs4 |= call->data[i-1] << (8 * (3 - ((i - 1) & 3)));

		if (((i - 1) & 3) != 0) {
			/* Still more codepoint to read */
			continue;
		}

		ucs4 = ntohl(ucs4); /* Convert codepoint to host endian */

		if (ucs4 < 0x80) {
			/* ASCII */
			*p++ = ucs4;
			ucs4 = 0;
		}

		/* Convert UCS4 to UTF-8, emitting the bytes in reverse order */
		while (ucs4 != 0) {
			/* 6 bits at a time */
			char c = ucs4 & 0x3f;
			ucs4 >>= 6;

			/* Inject the prefix */
			if (ucs4) {
				/* Continuation: fixed prefix */
				c |= 0x80;
			} else {
				/* Start: sign extend the start bits */
				c |= (((int8_t)0x80) >> (p - s));
			}
			*p++ = c;
		}
	}
	end = p;

	/* Reverse the UTF-8 encoded byte stream */
	while (p > outp) {
		char c = *outp;
		*(outp++) = *(--p);
		*p = c;
	}

	assert((size_t) (end - buf) == exp->len);
	assert(memcmp(buf, text, exp->len) == 0);
	free(buf);
}

void rufl_test_harness_assert_paint_calls(const char *fontname, ...)
{
	const rufl_test_harness_paint_call *call = NULL;
	va_list ap;

	va_start(ap, fontname);
	while (fontname != NULL) {
		rufl_test_harness_paint_call exp;
		const char *text;

		exp.font.name = fontname;
		exp.font.encoding = va_arg(ap, const char *);
		exp.font.xsize = va_arg(ap, int);
		exp.font.ysize = exp.font.xsize;
		/* XXX: This colludes with the xfont_find_font mock */
		exp.font.xres = exp.font.yres = 90;
		exp.xpos = va_arg(ap, int);
		exp.ypos = va_arg(ap, int);
		exp.ucs4 = va_arg(ap, int);
		exp.trfm = va_arg(ap, int);
		text = va_arg(ap, const char *);
		exp.len = strlen(text);

		call = rufl_test_harness_paint_enumerate(call);
		rufl_test_harness_assert_paint_call(call, &exp, text);

		fontname = va_arg(ap, const char *); /* Next fontname */
	}
	va_end(ap);

	/* Expect to have reached the end of the calls queue */
	call = rufl_test_harness_paint_enumerate(call);
	assert(call == NULL);
}

void rufl_test_harness_discard_paint_calls(void)
{
	while (h->paint_calls != NULL) {
		rufl_test_harness_paint_call_internal *next;
		next = h->paint_calls->next;
		free(h->paint_calls);
		h->paint_calls = next;
	}
	h->last_paint_call = NULL;
}
