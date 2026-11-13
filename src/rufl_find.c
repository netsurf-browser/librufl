/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 * Copyright 2005 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "rufl_internal.h"

static int rufl_family_list_cmp(const void *keyval, const void *datum);
static int rufl_font_list_cmp(const void *keyval, const void *datum);
static rufl_code rufl_place_in_cache(unsigned int font, unsigned int font_size,
		const char *encoding, font_f f);


/**
 * Given a Unicode codepoint and desired style, find substitute font with
   closest style.
 */
rufl_code rufl_find_alternate_font(unsigned int u, rufl_style font_style,
		unsigned int *font)
{
	unsigned int font1, font2;
	unsigned int weight = (font_style & 0xf) - 1;
	unsigned int slant = (font_style & rufl_SLANTED) ? 1 : 0;

	font1 = rufl_substitution_table_lookup(u);
	if (font1 != NOT_AVAILABLE &&
			(rufl_font_list[font1].weight != weight ||
			 rufl_font_list[font1].slant != slant)) {
		/* seek closer match, placing final answer in font1 */
		const char *font1_family;
		struct rufl_character_set *charset;
		rufl_code code;

		font1_family = rufl_family_list[rufl_font_list[font1].family];
		code = rufl_find_font_family(font1_family, font_style, &font2,
				NULL, &charset);
		if (code != rufl_OK)
			return code;
		if (font2 != font1 && charset &&
				rufl_character_set_test(charset, u))
			font1 = font2;
	}

	*font = font1;
	return rufl_OK;
}

static rufl_code rufl_find_font_family_by_identifier(const char *font_family,
		unsigned int *font)
{
	/* "font_family" contains a font identifier: direct lookup */
	const struct rufl_font_list_entry *fe;

	/* We only support a single qualifier */
	if (strchr(font_family + 1, '\\') != NULL)
		return rufl_FONT_NOT_FOUND;

	/* There must be at least one character after the qualifier */
	if (strlen(font_family) < 3)
		return rufl_FONT_NOT_FOUND;

	/* Ensure the qualifier is F */
	if (*(font_family + 1) != 'F')
		return rufl_FONT_NOT_FOUND;

	/* We have a valid identifier string. Search for the font */
	fe = bsearch(font_family+2, rufl_font_list,
			rufl_font_list_entries,
			sizeof rufl_font_list[0], rufl_font_list_cmp);
	if (!fe)
		return rufl_FONT_NOT_FOUND;

	*font = fe - rufl_font_list;

	return rufl_OK;
}

static rufl_code rufl_find_font_family_by_family_and_style(
		const char *font_family, rufl_style font_style,
		unsigned int *font)
{
	/* "font_family" is a family name: map via family map */
	struct rufl_family_map_entry_storage k;
	const struct rufl_family_map_entry_storage *e;
	const char **family;
	uint32_t f, slant, weight, gwidth, smallcaps;

	family = bsearch(font_family, rufl_family_list,
			rufl_family_list_entries,
			sizeof rufl_family_list[0],
			rufl_family_list_cmp);
	if (!family)
		return rufl_FONT_NOT_FOUND;

	gwidth = ((font_style >> 4) & 0xf);
	/* For backwards compatibility, map a gwidth of 0 to regular width */
	gwidth = (gwidth == 0) ? 2 : gwidth - 1;
	assert(gwidth <= 4);
	weight = (font_style & 0xf) - 1;
	assert(weight <= 8);
	smallcaps = 0; /*font_style & rufl_FLAG_CAPS ? 1 : 0;*/
	slant = font_style & rufl_FLAG_SLANTED ? 1 : 0;

	rufl_family_map_entry_populate(&k, family - rufl_family_list, weight,
			gwidth, slant, smallcaps, 0);

	/* Find an entry that matches the requested family and style */
	e = rufl_family_map_find(&k, rufl_family_map_cmp);
	if (e == NULL) {
		/* No exact match: search for the closest one */
		e = rufl_family_map_find_closest(&k);
		if (e == NULL)
			return rufl_FONT_NOT_FOUND;
	}

	rufl_family_map_entry_parse(e, NULL, NULL, NULL, NULL, NULL, &f);

	*font = f;

	return rufl_OK;
}

/**
 * Find a font family.
 */
rufl_code rufl_find_font_family(const char *font_family,
		rufl_style font_style, unsigned int *font,
		unsigned int *slanted, struct rufl_character_set **charset)
{
	unsigned int f;
	rufl_code code;

	if (!font_family)
		return rufl_FONT_NOT_FOUND;

	if (*font_family == '\\') {
		code = rufl_find_font_family_by_identifier(font_family, &f);
	} else {
		code = rufl_find_font_family_by_family_and_style(font_family,
				font_style, &f);
	}
	if (code != rufl_OK)
		return code;

	if (font)
		(*font) = f;

	if (slanted)
		(*slanted) = rufl_font_list[f].slant;

	if (charset)
		(*charset) = rufl_font_list[f].charset;

	return rufl_OK;
}


/**
 * Find a sized font, placing in the cache if necessary.
 */
rufl_code rufl_find_font(unsigned int font, unsigned int font_size,
		const char *encoding, font_f *fhandle)
{
	font_f f;
	char font_name[80];
	unsigned int i;
	rufl_code code;

	assert(fhandle != NULL);

	for (i = 0; i != rufl_CACHE_SIZE; i++) {
		/* Comparing pointers for the encoding is fine, as the 
		 * encoding string passed to us is either:
		 *
		 *    a) NULL
		 * or b) statically allocated
		 * or c) resides in the font's umap, which is constant
		 *       for the lifetime of the application.
		 */
		if (rufl_cache[i].font == font &&
				rufl_cache[i].size == font_size &&
				rufl_cache[i].encoding == encoding)
			break;
	}
	if (i != rufl_CACHE_SIZE) {
		/* found in cache */
		f = rufl_cache[i].f;
		rufl_cache[i].last_used = rufl_cache_time++;
	} else {
		/* not found */
		if (font == rufl_CACHE_CORPUS) {
			if (encoding)
				snprintf(font_name, sizeof font_name,
					"Corpus.Medium\\E%s", encoding);
			else
				snprintf(font_name, sizeof font_name,
					"Corpus.Medium");
		} else {
			if (encoding)
				snprintf(font_name, sizeof font_name,
					"%s\\E%s",
					rufl_font_list[font].identifier,
					encoding);
			else
				snprintf(font_name, sizeof font_name, "%s",
					rufl_font_list[font].identifier);
		}

		rufl_fm_error = xfont_find_font(font_name,
				font_size, font_size, 0, 0, &f, 0, 0);
		if (rufl_fm_error) {
			LOG("xfont_find_font: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
		/* place in cache */
		code = rufl_place_in_cache(font, font_size, encoding, f);
		if (code != rufl_OK)
			return code;
	}

	(*fhandle) = f;

	return rufl_OK;
}


int rufl_family_list_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const char * const *entry = datum;
	return strcasecmp(key, *entry);
}


int rufl_font_list_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const struct rufl_font_list_entry * const entry = datum;
	return strcasecmp(key, entry->identifier);
}

/**
 * Place a font into the recent-use cache, making space if necessary.
 */

rufl_code rufl_place_in_cache(unsigned int font, unsigned int font_size,
		const char *encoding, font_f f)
{
	unsigned int i;
	unsigned int max_age = 0;
	unsigned int evict = 0;

	for (i = 0; i != rufl_CACHE_SIZE; i++) {
		if (rufl_cache[i].font == rufl_CACHE_NONE) {
			evict = i;
			break;
		} else if (max_age < rufl_cache_time -
				rufl_cache[i].last_used) {
			max_age = rufl_cache_time -
					rufl_cache[i].last_used;
			evict = i;
		}
	}
	if (rufl_cache[evict].font != rufl_CACHE_NONE) {
		rufl_fm_error = xfont_lose_font(rufl_cache[evict].f);
		if (rufl_fm_error)
			return rufl_FONT_MANAGER_ERROR;
	}
	rufl_cache[evict].font = font;
	rufl_cache[evict].size = font_size;
	rufl_cache[evict].encoding = encoding;
	rufl_cache[evict].f = f;
	rufl_cache[evict].last_used = rufl_cache_time++;

	return rufl_OK;
}
