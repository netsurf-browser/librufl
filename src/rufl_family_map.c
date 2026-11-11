/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2026 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>
#include "rufl_internal.h"

/** Size of rufl_family_map entries */
enum {
	rufl_FAMILY_MAP_8bit = 0,
	rufl_FAMILY_MAP_16bit = (1 << 15)
};

/** Base object for an entry in rufl_family_map */
struct rufl_family_map_entry_base {
	/** Composite value, as follows:
	 *
	 *  b0    : slanted
	 *  b1-3  : reserved (0)
	 *  b4-7  : weight
	 *  b8-14 : reserved (0)
	 *  b15   : entry type (0 for 8bit; 1 for 16bit)
	 */
	uint16_t bits;
	/* Entry value part: either a single 16bit family, or two 8bit
	 * indexes (one for the family, the other for the font).
	 * Bit 15 of "bits" distinguishes between them.
	 */
	union {
		uint16_t family;
		struct {
			uint8_t family;
			uint8_t font;
		} b8;
	} u;
};

/** An entry in rufl_family_map */
struct rufl_family_map_entry {
	struct rufl_family_map_entry_base base;

	/* If the 16bit flag is set, then these two fields follow.
	 * If the 16bit flag is *not* set, then they do not (and the
	 * family map is an array of rufl_family_map_entry_base objects)
	 */

	/** Font identifier  */
	uint16_t font;
	/** Padding (0) */
	uint16_t pad;
};


/**
 * Exact match a rufl_family_map_entry
 *
 * This matches the entire (family, bits) tuple.
 */
int rufl_family_map_cmp(const void *e1, const void *e2)
{
	const struct rufl_family_map_entry *key = e1;
	const struct rufl_family_map_entry *entry = e2;
	uint32_t k1, k2;

	assert((key->base.bits & rufl_FAMILY_MAP_16bit) ==
			(entry->base.bits & rufl_FAMILY_MAP_16bit));

	if (key->base.bits & rufl_FAMILY_MAP_16bit) {
		k1 = (key->base.u.family << 16) | key->base.bits;
		k2 = (entry->base.u.family << 16) | entry->base.bits;
	} else {
		k1 = (key->base.u.b8.family << 16) | key->base.bits;
		k2 = (entry->base.u.b8.family << 16) | entry->base.bits;
	}

	if (k1 < k2)
		return -1;
	else if (k1 > k2)
		return 1;
	return 0;
}


/**
 * Fuzzy match a family in a rufl_family_map_entry
 *
 * This only considers whether the family matches, paying
 * no heed to any more specific qualifiers.
 */
int rufl_family_map_family_cmp(const void *e1, const void *e2)
{
	const struct rufl_family_map_entry *key = e1;
	const struct rufl_family_map_entry *entry = e2;
	uint32_t k1, k2;

	assert((key->base.bits & rufl_FAMILY_MAP_16bit) ==
			(entry->base.bits & rufl_FAMILY_MAP_16bit));

	if (key->base.bits & rufl_FAMILY_MAP_16bit) {
		k1 = (key->base.u.family << 16);
		k2 = (entry->base.u.family << 16);
	} else {
		k1 = (key->base.u.b8.family << 16);
		k2 = (entry->base.u.b8.family << 16);
	}

	if (k1 < k2)
		return -1;
	else if (k1 > k2)
		return 1;
	return 0;
}


/**
 * Populate a rufl_family_map_entry
 */
void rufl_family_map_entry_populate(struct rufl_family_map_entry_storage *e,
		uint32_t family, uint32_t weight, uint32_t slanted,
		uint32_t font)
{
	struct rufl_family_map_entry *i = (struct rufl_family_map_entry *) e;

	assert(weight <= 8);
	assert(slanted <= 1);
	assert(family < rufl_family_list_entries);
	assert(font < rufl_font_list_entries);

	if (rufl_family_list_entries > 256 || rufl_font_list_entries > 256) {
		i->base.bits = rufl_FAMILY_MAP_16bit | (weight << 4) | slanted;
		i->base.u.family = family;
		i->font = font;
	} else {
		i->base.bits = (weight << 4) | slanted;
		i->base.u.b8.family = family;
		i->base.u.b8.font = font;
	}
}


/**
 * Extract the contents of a rufl_family_map_entry
 */
void rufl_family_map_entry_parse(const struct rufl_family_map_entry_storage *e,
		uint32_t *family, uint32_t *weight, uint32_t *slanted,
		uint32_t *font)
{
	const struct rufl_family_map_entry *i =
			(const struct rufl_family_map_entry *) e;

	if (weight != NULL)
		*weight = (i->base.bits >> 4) & 0xf;
	if (slanted != NULL)
		*slanted = i->base.bits & 0x1;

	if (i->base.bits & rufl_FAMILY_MAP_16bit) {
		if (family != NULL)
			*family = i->base.u.family;
		if (font != NULL)
			*font = i->font;
	} else {
		if (family != NULL)
			*family = i->base.u.b8.family;
		if (font != NULL)
			*font = i->base.u.b8.font;
	}
}


/**
 * Compute the address of the previous family map entry
 */
struct rufl_family_map_entry_storage *rufl_family_map_prev(
		const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry *i =
			(const struct rufl_family_map_entry *) e;

	if (e <= rufl_family_map) {
		return NULL;
	}

	if (i->base.bits & rufl_FAMILY_MAP_16bit) {
		return (struct rufl_family_map_entry_storage *)(void *) (i - 1);
	} else {
		const struct rufl_family_map_entry_base *b = &i->base;
		return (struct rufl_family_map_entry_storage *)(void *) (b - 1);
	}
}


/**
 * Compute the address of the next family map entry
 */
struct rufl_family_map_entry_storage *rufl_family_map_next(
		const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry *i =
			(const struct rufl_family_map_entry *) e;
	const struct rufl_family_map_entry *last;
	struct rufl_family_map_entry *next;

	if (i->base.bits & rufl_FAMILY_MAP_16bit) {
		const struct rufl_family_map_entry *map =
			(const struct rufl_family_map_entry *) rufl_family_map;
		next = (struct rufl_family_map_entry *) (i + 1);
		last = &map[rufl_family_map_entries - 1];
	} else {
		const struct rufl_family_map_entry_base *b = &i->base;
		const struct rufl_family_map_entry_base *map =
				(const struct rufl_family_map_entry_base *)
				rufl_family_map;
		next = (struct rufl_family_map_entry *) (b + 1);
		last = (const struct rufl_family_map_entry *)
				&map[rufl_family_map_entries - 1];
	}

	if (next >= last) {
		return NULL;
	}

	return (struct rufl_family_map_entry_storage *)(void *) next;
}

/**
 * Search for an entry in the rufl_family_map
 */
struct rufl_family_map_entry_storage *rufl_family_map_find(
		const struct rufl_family_map_entry_storage *k,
		int (*cmpfunc)(const void *, const void *))
{
	struct rufl_family_map_entry *map =
			(struct rufl_family_map_entry *) rufl_family_map;
	size_t fmesize;

	if (rufl_family_map_entries == 0) {
		return NULL;
	}

	if (map[0].base.bits & rufl_FAMILY_MAP_16bit) {
		fmesize = sizeof(struct rufl_family_map_entry);
	} else {
		fmesize = sizeof(struct rufl_family_map_entry_base);
	}

	return bsearch(k, rufl_family_map, rufl_family_map_entries, fmesize,
			cmpfunc);
}


/**
 * Append an item to the rufl_family_map
 */
rufl_code rufl_family_map_append(const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry *prev;
	struct rufl_family_map_entry *tmp;
	size_t fmesize;
	uint32_t family, weight, slanted, font;

	if (rufl_family_list_entries > 256 || rufl_font_list_entries > 256) {
		fmesize = sizeof(struct rufl_family_map_entry);
	} else {
		fmesize = sizeof(struct rufl_family_map_entry_base);
	}

	tmp = realloc(rufl_family_map, (rufl_family_map_entries + 1) * fmesize);
	if (tmp == NULL) {
		return rufl_OUT_OF_MEMORY;
	}
	rufl_family_map = (struct rufl_family_map_entry_storage *)(void *) tmp;

	if (rufl_family_list_entries > 256 || rufl_font_list_entries > 256) {
		struct rufl_family_map_entry *map =
			(struct rufl_family_map_entry *) rufl_family_map;
		prev = &map[rufl_family_map_entries - 1];
		tmp = &map[rufl_family_map_entries];
	} else {
		struct rufl_family_map_entry_base *b =
			(struct rufl_family_map_entry_base *) rufl_family_map;
		prev = (const struct rufl_family_map_entry *)
				&b[rufl_family_map_entries - 1];
		tmp = (struct rufl_family_map_entry *)
				&b[rufl_family_map_entries];
	}

	rufl_family_map_entry_parse(e, &family, &weight, &slanted, &font);
	rufl_family_map_entry_populate(
			(struct rufl_family_map_entry_storage *)(void *) tmp,
			family, weight, slanted, font);

	rufl_family_map_entries++;

	if (rufl_family_map_entries > 1 && rufl_family_map_cmp(tmp, prev) < 0) {
		/* Sort the family map as the new entry is out of place */
		qsort(rufl_family_map, rufl_family_map_entries, fmesize,
			rufl_family_map_cmp);
	}

	return rufl_OK;
}
