#include <assert.h>
#include <stdlib.h>

#include "harness-priv.h"

rufl_test_harness_t *h = NULL;

static void rufl_test_harness_free(void)
{
	free(h->font_names);
	free(h->encodings);
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

	names = realloc(h->font_names,
			(h->n_font_names + 1) * sizeof(*names));
	assert(names != NULL);

	h->font_names = names;

	h->font_names[h->n_font_names++] = name;
}

void rufl_test_harness_register_encoding(const char *encoding)
{
	const char **encodings;

	encodings = realloc(h->encodings,
			(h->n_encodings + 1) * sizeof(*encodings));
	assert(encodings != NULL);

	h->encodings = encodings;

	h->encodings[h->n_encodings++] = encoding;
}

void rufl_test_harness_set_font_encoding(const char *path)
{
	h->encoding_filename = path;
}
