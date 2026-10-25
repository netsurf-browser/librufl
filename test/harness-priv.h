#ifndef rufl_test_harness_priv_h_
#define rufl_test_harness_priv_h_

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#include "harness.h"

typedef struct {
	unsigned int refcnt;
	size_t name; /* Index of name in names array */
#define FONT_ENCODING_SYMBOL ((size_t) -1) /* Symbol, not language, font */
	size_t encoding; /* Index of encoding in encodings array */
	int xsize; /* XSize of this font */
	int ysize; /* YSize if this font */
	int xres; /* XResolution of this font */
	int yres; /* YResolution of this font */
} rufl_test_harness_sized_font;

typedef struct {
	int codepoint; /* UCS codepoint */
	int index; /* Glyph index (or -1 for unmapped) */
} rufl_test_harness_glyph_map_entry;

typedef struct {
	size_t n_entries;
	rufl_test_harness_glyph_map_entry entries[];
} rufl_test_harness_glyph_map;

typedef struct rufl_test_harness_paint_call_internal {
	struct rufl_test_harness_paint_call_internal *next;
	size_t name;
	size_t encoding;
	/* Must be last, as variable length */
	rufl_test_harness_paint_call pub;
} rufl_test_harness_paint_call_internal;

typedef struct {
	int fm_version;
	bool fm_ucs;
	bool fm_broken_fec;

	const char **font_names;
	size_t n_font_names;

	const char **encodings;
	size_t n_encodings;

	/* n_font_names * (n_encodings + 1) entries */
	char **encoding_filenames;

	/* n_font_names entries */
	rufl_test_harness_glyph_map **glyph_maps;

	/* At most 256 active font handles */
	rufl_test_harness_sized_font fonts[256];
	int current_font;

	char *buffer;
	int buffer_flags;

	rufl_test_harness_paint_call_internal *paint_calls;
	rufl_test_harness_paint_call_internal *last_paint_call;
} rufl_test_harness_t;

extern rufl_test_harness_t *h;

#endif
