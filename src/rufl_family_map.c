/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2026 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rufl_internal.h"

enum {
	/** Size of rufl_family_map entries */
	rufl_FAMILY_MAP_8bit = 0,
	rufl_FAMILY_MAP_16bit = (1 << 15),
	/** Font variant flags */
	rufl_FAMILY_MAP_VARIANT_SC = 0x4000,
	rufl_FAMILY_MAP_VARIANT_MASK = 0x7000,
};

/** Base object for an entry in rufl_family_map */
struct rufl_family_map_entry_base {
	/** Composite value, as follows:
	 *
	 *  b0    : slanted
	 *  b1-3  : reserved (0)
	 *  b4-7  : weight
	 *  b8-10 : glyph width
	 *  b11-13: reserved (0)
	 *  b14   : smallcaps
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
	const struct rufl_family_map_entry *key = e1;
	const struct rufl_family_map_entry *entry = e2;
	uint32_t k1, k2;

	assert((key->base.bits & rufl_FAMILY_MAP_16bit) ==
			(entry->base.bits & rufl_FAMILY_MAP_16bit));

	if (key->base.bits & rufl_FAMILY_MAP_16bit) {
		k1 = (key->base.value << 16) | key->base.bits;
		k2 = (entry->base.value << 16) | entry->base.bits;
	} else {
		k1 = ((key->base.value & 0xff) << 16) | key->base.bits;
		k2 = ((entry->base.value & 0xff) << 16) | entry->base.bits;
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
	const struct rufl_family_map_entry *key = e1;
	const struct rufl_family_map_entry *entry = e2;
	uint32_t k1, k2;

	assert((key->base.bits & rufl_FAMILY_MAP_16bit) ==
			(entry->base.bits & rufl_FAMILY_MAP_16bit));

	if (key->base.bits & rufl_FAMILY_MAP_16bit) {
		k1 = (key->base.value << 16);
		k2 = (entry->base.value << 16);
	} else {
		k1 = ((key->base.value & 0xff) << 16);
		k2 = ((entry->base.value & 0xff) << 16);
	}

	return (int32_t) (k1 - k2);
}


/**
 * Populate a rufl_family_map_entry
 */
void rufl_family_map_entry_populate(struct rufl_family_map_entry_storage *e,
		uint32_t family, uint32_t weight, uint32_t gwidth,
		uint32_t slanted, uint32_t smallcaps, uint32_t font)
{
	struct rufl_family_map_entry *i = (struct rufl_family_map_entry *) e;

	assert(weight <= 8);
	assert(gwidth <= 4);
	assert(slanted <= 1);
	assert(smallcaps <= 1);
	assert(family < rufl_family_list_entries);
	assert(font < rufl_font_list_entries);

	i->base.bits = (smallcaps << 14)
			| (gwidth << 8)
			| (weight << 4)
			| slanted;

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
		uint32_t *family, uint32_t *weight, uint32_t *gwidth,
		uint32_t *slanted, uint32_t *smallcaps, uint32_t *font)
{
	const struct rufl_family_map_entry *i =
			(const struct rufl_family_map_entry *) e;

	if (weight != NULL)
		*weight = (i->base.bits >> 4) & 0xf;
	if (gwidth != NULL)
		*gwidth = (i->base.bits >> 8) & 0x7;
	if (slanted != NULL)
		*slanted = i->base.bits & 0x1;
	if (smallcaps != NULL)
		*smallcaps = (i->base.bits >> 14) & 0x1;

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


static struct rufl_family_map_entry_storage *find_closest(
		const struct rufl_family_map_entry_storage *k)
{
	struct rufl_family_map_entry_storage target = *k;
	struct rufl_family_map_entry_storage *e, *best = NULL;
	int best_distance;
	uint32_t weight, ncomponents;

#ifdef FM_DEBUG
	{
		uint32_t tfam, tgw, tw, tsc, ts;
		rufl_family_map_entry_parse(&target, &tfam, &tw, &tgw,
				&ts, &tsc, NULL);
		printf("Want: %s %u %u %u %u\n", rufl_family_list[tfam],
				tsc, tgw, tw, ts);
	}
#endif

	/* Capture the requested weight: we need this later */
	rufl_family_map_entry_parse(&target, NULL, &weight, NULL, NULL, NULL,
			NULL);

	/* Find an arbitrary entry for the requested family */
	e = rufl_family_map_find(&target, rufl_family_map_family_cmp);
	assert(e != NULL);

	/* Try to improve the match. We perform a pass per component */
	for (ncomponents = 4; ncomponents > 0; ncomponents--) {
		if (best != NULL) {
			uint32_t tfam, tgw, tw, tsc, ts;
			uint32_t bgw, bw, bsc, bs;

			/* Obtain the target's component values */
			rufl_family_map_entry_parse(&target, &tfam, &tw, &tgw,
					&ts, &tsc, NULL);

			/* Obtain the best match's component values */
			rufl_family_map_entry_parse(best, NULL, &bw, &bgw,
					&bs, &bsc, NULL);

			/* Finalise the target value for the most
			 * significant component considered by the
			 * previous pass. The order here *must* match
			 * the order of the fields in "bits"; most
			 * significant first. */
			switch (ncomponents) {
			case 3: tsc = bsc; break;
			case 2:	tgw = bgw; break;
			case 1:	tw = bw; break;
			/* case 0: ts = bs; break; -- Never reached */
			}

#ifdef FM_DEBUG
			{
				printf("Adjusted target: %s %u %u %u %u\n",
					rufl_family_list[tfam],
					tsc, tgw, tw, ts);
			}
#endif

			/* Set the new target */
			rufl_family_map_entry_populate(&target, tfam, tw, tgw,
					ts, tsc, 0);

			/* Start this pass from the best match so far */
			e = best;
		}
		best_distance = INT_MAX;

		/* Search away from the current item to find the
		 * closest match to the target within the same family */
		while (e != NULL && rufl_family_map_family_cmp(e,
				&target) == 0) {
			int d;

			/* Compute distance to target */
			d = rufl_family_map_cmp(e, &target);

#ifdef FM_DEBUG
			{
				uint32_t ft, gw, w, sc, s;
				rufl_family_map_entry_parse(e, NULL,
						&w, &gw, &s, &sc, &ft);

				printf("%u: Considering: %s %u %u %u %u (%d)",
						ncomponents,
						rufl_font_list[ft].identifier,
						sc, gw, w, s, d);
				if (best != NULL) {
					uint32_t bft, bgw, bw, bsc, bs;
					rufl_family_map_entry_parse(best, NULL,
						&bw, &bgw, &bs, &bsc, &bft);
					printf(" Best: %s %u %u %u %u (%d)",
						rufl_font_list[bft].identifier,
						bsc,bgw, bw, bs, best_distance);
				}
				printf("\n");
			}
#endif

			/* Update best match, if appropriate */
			if (abs(d) < abs(best_distance)) {
				best = e;
				best_distance = d;
				/* Stop if we have an exact match */
				if (d == 0)
					break;
			} else if (abs(d) == abs(best_distance)) {
				/* Two candidates, of equal legitimacy.
				 * below Regular weight, favour lighter
				 */
				if ((weight <= 2 && d < best_distance) ||
						(weight > 2 &&
						 d > best_distance)) {
					best = e;
					best_distance = d;
				}
				/* We cannot get closer, however */
				break;
			} else {
				/* Current candidate is worse: done */
				break;
			}

			/* Select next entry to process */
			if (d < 0) {
				/* Candidate is below target */
				e = rufl_family_map_next(e);
			} else {
				/* Candidate is above target */
				e = rufl_family_map_prev(e);
			}
		}
	}

#ifdef FM_DEBUG
	{
		uint32_t ft, gw, w, sc, s;
		rufl_family_map_entry_parse(best, NULL, &w, &gw, &s, &sc, &ft);
		printf("Chose: %s %u %u %u %u\n", rufl_font_list[ft].identifier,
				sc, gw, w, s);
	}
#endif

	return best;
}


/**
 * Search for the closest entry in the rufl_family_map
 */
struct rufl_family_map_entry_storage *rufl_family_map_find_closest(
		const struct rufl_family_map_entry_storage *k)
{
	struct rufl_family_map_entry_storage target;
	struct rufl_family_map_entry_storage *e = &target;

	/* Initialise target from client parameter, taking care to
	 * copy only the amount of data guaranteed to be available. */
	if (((struct rufl_family_map_entry_base *) k)->bits &
			rufl_FAMILY_MAP_16bit) {
		*((struct rufl_family_map_entry *) e) =
				*((struct rufl_family_map_entry *) k);
	} else {
		*((struct rufl_family_map_entry_base *) e) =
				*((struct rufl_family_map_entry_base *) k);
	}
	/* Neutralise bits that can never be searched for via this API */
	((struct rufl_family_map_entry_base *) e)->bits &=
			~rufl_FAMILY_MAP_VARIANT_MASK;

	e = find_closest(&target);
	if (e != NULL && ((struct rufl_family_map_entry_base *) e)->bits &
			rufl_FAMILY_MAP_VARIANT_MASK) {
		/* Closest match has a variant bit set: reject it */
		e = NULL;
	}
	return e;
}


/**
 * Search for the closest entry in the rufl_family_map with a glyph for the
 * specified codepoint
 */
struct rufl_family_map_entry_storage *
rufl_family_map_find_closest_with_codepoint(
		const struct rufl_family_map_entry_storage *k, uint32_t ucs4)
{
	struct rufl_family_map_entry_storage target = {0};
	struct rufl_family_map_entry_storage *e, *e1 = &target;

	/* Initialise target from client parameter, taking care to
	 * copy only the amount of data guaranteed to be available. */
	if (((struct rufl_family_map_entry_base *) k)->bits &
			rufl_FAMILY_MAP_16bit) {
		*((struct rufl_family_map_entry *) e1) =
				*((struct rufl_family_map_entry *) k);
	} else {
		*((struct rufl_family_map_entry_base *) e1) =
				*((struct rufl_family_map_entry_base *) k);
	}
	/* Clear client-provided variant bits, which cannot have effect here. */
	((struct rufl_family_map_entry_base *) e1)->bits &=
			~rufl_FAMILY_MAP_VARIANT_MASK;

	/* Try without variant */
	e = find_closest(&target);
	if (e != NULL) {
		const struct rufl_font_list_entry *f;
		unsigned int font;

		rufl_family_map_entry_parse(e, NULL, NULL, NULL, NULL, NULL,
				&font);

		f = &rufl_font_list[font];

		if (f->charset == NULL || !rufl_character_set_test(f->charset,
				ucs4)) {
			/* Chosen entry has no glyph for the codepoint */
			e = NULL;
		}
	}

	/* Try variants */
	((struct rufl_family_map_entry_base *) e1)->bits |=
			rufl_FAMILY_MAP_VARIANT_SC;
	e1 = find_closest(&target);
	if (e1 != NULL) {
		const struct rufl_font_list_entry *f;
		unsigned int font;

		rufl_family_map_entry_parse(e1, NULL, NULL, NULL, NULL, NULL,
				&font);

		f = &rufl_font_list[font];

		if (f->charset == NULL || !rufl_character_set_test(f->charset,
				ucs4)) {
			/* Chosen entry has no glyph for the codepoint */
			e1 = NULL;
		}
	}

	/* Return the closest of the options we have */
	if (e != NULL && e1 != NULL) {
		struct rufl_family_map_entry_storage norm = {0};
		struct rufl_family_map_entry_storage *e1n = &norm;
		int d, d1;

		if (((struct rufl_family_map_entry_base *) e1)->bits &
				rufl_FAMILY_MAP_16bit) {
			*((struct rufl_family_map_entry *) e1n) =
					*((struct rufl_family_map_entry *) e1);
		} else {
			*((struct rufl_family_map_entry_base *) e1n) =
				*((struct rufl_family_map_entry_base *) e1);
		}
		((struct rufl_family_map_entry_base *) e1n)->bits &=
				~rufl_FAMILY_MAP_VARIANT_MASK;

		d = rufl_family_map_cmp(e, &target);

		d1 = rufl_family_map_cmp(e1n, &target);

		/* Prefer non-variant font if identical distance from target */
		if (abs(d1) < abs(d)) {
			e = e1;
		}
	} else if (e1 != NULL) {
		e = e1;
	}

	return e;
}


/**
 * Append an item to the rufl_family_map
 */
rufl_code rufl_family_map_append(const struct rufl_family_map_entry_storage *e)
{
	const struct rufl_family_map_entry *prev;
	struct rufl_family_map_entry *tmp;
	size_t fmesize;
	uint32_t family, weight, gwidth, slanted, smallcaps, font;

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

	rufl_family_map_entry_parse(e, &family, &weight, &gwidth,
			&slanted, &smallcaps, &font);
	rufl_family_map_entry_populate(
			(struct rufl_family_map_entry_storage *)(void *) tmp,
			family, weight, gwidth, slanted, smallcaps, font);

	rufl_family_map_entries++;

	if (rufl_family_map_entries > 1 && rufl_family_map_cmp(tmp, prev) < 0) {
		/* Sort the family map as the new entry is out of place */
		qsort(rufl_family_map, rufl_family_map_entries, fmesize,
			rufl_family_map_cmp);
	}

	return rufl_OK;
}
