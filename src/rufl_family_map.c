/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2026 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	uint16_t value;
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
/* Statically assert that the public storage representation is large enough */
extern char rufl_family_map_storage_too_small[
		sizeof(struct rufl_family_map_entry) <=
		sizeof(struct rufl_family_map_entry_storage) ? 1: -1];


/**
 * Exact match a rufl_family_map_entry
 *
 * This matches the entire (family, bits) tuple.
 */
int rufl_family_map_cmp(const void *e1, const void *e2)
{
	const struct rufl_family_map_entry_base *key = e1;
	const struct rufl_family_map_entry_base *entry = e2;
	uint32_t k1, k2;

	assert((key->bits & rufl_FAMILY_MAP_16bit) ==
			(entry->bits & rufl_FAMILY_MAP_16bit));

	k1 = (key->value << 16) | key->bits;
	k2 = (entry->value << 16) | entry->bits;

	if ((key->bits & rufl_FAMILY_MAP_16bit) == 0) {
		k1 &= 0x00ffffff;
		k2 &= 0x00ffffff;
	}

	return (int32_t) (k1 - k2);
}


/**
 * Fuzzy match a family in a rufl_family_map_entry
 *
 * This only considers whether the family matches, paying
 * no heed to any more specific qualifiers.
 */
int rufl_family_map_family_cmp(const void *e1, const void *e2)
{
	const struct rufl_family_map_entry_base *key = e1;
	const struct rufl_family_map_entry_base *entry = e2;
	uint32_t k1, k2;

	assert((key->bits & rufl_FAMILY_MAP_16bit) ==
			(entry->bits & rufl_FAMILY_MAP_16bit));

	k1 = (key->value << 16);
	k2 = (entry->value << 16);

	if ((key->bits & rufl_FAMILY_MAP_16bit) == 0) {
		k1 &= 0x00ffffff;
		k2 &= 0x00ffffff;
	}

	return (int32_t) (k1 - k2);
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

	i->base.bits = (weight << 4) | slanted;

	if (rufl_family_list_entries > 256 || rufl_font_list_entries > 256) {
		i->base.bits |= rufl_FAMILY_MAP_16bit;
		i->base.value = family;
		i->font = font;
	} else {
		i->base.value = (font << 8) | family;
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
			*family = i->base.value;
		if (font != NULL)
			*font = i->font;
	} else {
		if (family != NULL)
			*family = i->base.value & 0xff;
		if (font != NULL)
			*font = (i->base.value >> 8);
	}
}


/**
 * Compute the address of the previous family map entry
 */
struct rufl_family_map_entry_storage *rufl_family_map_prev(
		const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry_base *i =
			(const struct rufl_family_map_entry_base *) e;

	if (e <= rufl_family_map) {
		return NULL;
	}

	if (i->bits & rufl_FAMILY_MAP_16bit) {
		i = &(((const struct rufl_family_map_entry *) e) - 1)->base;
	} else {
		i -= 1;
	}

	return (struct rufl_family_map_entry_storage *) i;
}


/**
 * Compute the address of the next family map entry
 */
struct rufl_family_map_entry_storage *rufl_family_map_next(
		const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry_base *i =
			(const struct rufl_family_map_entry_base *) e;
	const struct rufl_family_map_entry_base *last;
	const struct rufl_family_map_entry_base *next;

	if (i->bits & rufl_FAMILY_MAP_16bit) {
		next = &(((const struct rufl_family_map_entry *) e) + 1)->base;
		last = &(((const struct rufl_family_map_entry *)
			  rufl_family_map) + rufl_family_map_entries - 1)->base;
	} else {
		next = i + 1;
		last = ((const struct rufl_family_map_entry_base *)
				rufl_family_map) + rufl_family_map_entries - 1;
	}

	if (next > last) {
		return NULL;
	}

	return (struct rufl_family_map_entry_storage *) next;
}

/**
 * Search for an entry in the rufl_family_map
 */
struct rufl_family_map_entry_storage *rufl_family_map_find(
		const struct rufl_family_map_entry_storage *k,
		int (*cmpfunc)(const void *, const void *))
{
	const struct rufl_family_map_entry_base *i =
			(const struct rufl_family_map_entry_base *) k;
	size_t fmesize;

	if (rufl_family_map_entries == 0) {
		return NULL;
	}

	if (i->bits & rufl_FAMILY_MAP_16bit) {
		fmesize = sizeof(struct rufl_family_map_entry);
	} else {
		fmesize = sizeof(struct rufl_family_map_entry_base);
	}

	return bsearch(k, rufl_family_map, rufl_family_map_entries, fmesize,
			cmpfunc);
}


/**
 * Search for the closest entry in the rufl_family_map
 */
struct rufl_family_map_entry_storage *rufl_family_map_find_closest(
		const struct rufl_family_map_entry_storage *k)
{
	struct rufl_family_map_entry_storage *e, *best = NULL;
	int32_t best_distance = INT32_MAX;
	uint32_t tw, ts;

	/* Parse target: we need these values later */
	rufl_family_map_entry_parse(k, NULL, &tw, &ts, NULL);

	/* Find an arbitrary entry for this family */
	e = rufl_family_map_find(k, rufl_family_map_family_cmp);

	/* Try to improve the match */
	while (e != NULL && rufl_family_map_family_cmp(e, k) == 0) {
		int32_t d;

		d = rufl_family_map_cmp(e, k);

		if (abs(d) < abs(best_distance)) {
			best = e;
			best_distance = d;
		} else if (abs(d) == abs(best_distance)) {
			/* Two candidates, of equal legitimacy */
			/* below Regular weight, favour lighter */
			if ((tw <= 2 && d < best_distance) ||
					(tw > 2 && d > best_distance)) {
				best = e;
			}
			/* No further improvement to be had: done */
			break;
		} else {
			/* Current candidate is worse: done */
			break;
		}

		if (d < 0) {
			/* Candidate is below target */
			e = rufl_family_map_next(e);
		} else {
			/* Candidate is above target */
			e = rufl_family_map_prev(e);
		}
	}

	if (best != NULL) {
		/* We have discovered the best match, but the slant
		 * may not meet requirements. If it differs, look for
		 * the same weight with the matching slant and prefer it.
		 */
		uint32_t w, s;
		rufl_family_map_entry_parse(best, NULL, &w, &s, NULL);
		if (s != ts) {
			if (s > ts) {
				/* Candidate has slant, but target does not */
				e = rufl_family_map_prev(best);
			} else {
				/* Target has slant, but candidate does not */
				e = rufl_family_map_next(best);
			}
			if (e != NULL) {
				uint32_t cw, cs;
				rufl_family_map_entry_parse(e, NULL, &cw, &cs,
						NULL);
				if (cw == w && cs == ts &&
						rufl_family_map_family_cmp(e,
							k) == 0) {
					/* Same family/weight; better slant */
					best = e;
				}
			}
		}
	}

	return best;
}


/**
 * Append an item to the rufl_family_map
 */
rufl_code rufl_family_map_append(const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry_storage *prev;
	void *tmp;
	size_t fmesize;

	if (((const struct rufl_family_map_entry *) e)->base.bits &
			rufl_FAMILY_MAP_16bit) {
		fmesize = sizeof(struct rufl_family_map_entry);
	} else {
		fmesize = sizeof(struct rufl_family_map_entry_base);
	}

	tmp = realloc(rufl_family_map, (rufl_family_map_entries + 1) * fmesize);
	if (tmp == NULL) {
		return rufl_OUT_OF_MEMORY;
	}
	rufl_family_map = tmp;

	tmp = memcpy(((char *) rufl_family_map) +
			(rufl_family_map_entries * fmesize), e, fmesize);

	prev = rufl_family_map_prev(tmp);

	rufl_family_map_entries++;

#ifdef FM_DEBUG
	{
		uint32_t family, weight, slanted, font;

		rufl_family_map_entry_parse(e, &family, &weight, &slanted,
				&font);

		printf("Adding %d %d %d:'%s' (%d: '%s')\n",
				weight, slanted,
				family, rufl_family_list[family],
				font, rufl_font_list[font].identifier);
	}
#endif

	if (rufl_family_map_entries > 1 && rufl_family_map_cmp(tmp, prev) < 0) {
		/* Sort the family map as the new entry is out of place */
		qsort(rufl_family_map, rufl_family_map_entries, fmesize,
			rufl_family_map_cmp);
	}

	return rufl_OK;
}
