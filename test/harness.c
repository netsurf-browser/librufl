#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness-priv.h"

rufl_test_harness_t *h = NULL;

static void rufl_test_harness_free(void)
{
	size_t ni, ei;

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
