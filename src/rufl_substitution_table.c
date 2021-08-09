/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "rufl_internal.h"

#undef RUFL_SUBSTITUTION_TABLE_DEBUG

/**
 * A perfect hash constructed at library initialisation time using the
 * CHD algorithm. Hash entries are found via a two-step process:
 *
 *   1. apply a first-stage hash to the key to find the bucket
 *      in which the corresponding entry should be found.
 *   2. apply a second-stage hash to the key and the stored
 *      displacement value for the bucket to find the index
 *      into the substitution table.
 */
struct rufl_substitution_table {
	uint32_t num_buckets; /**< Number of buckets in the hash */
	uint32_t num_slots; /**< Number of slots in the table */
	/** Substitution table.
	 *
	 * Fields in the substitution table have the following format:
	 *
	 *    3                   2                   1                   0
	 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |      Reserved       |            Unicode codepoint            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |            Reserved           |        Font identifier        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 * where:
	 *
	 *   reserved: 11 bits/16 bits
	 *     These bits are currently unused and must be set to 0.
	 *
	 *   unicode codepoint: 21 bits
	 *     The Unicode codepoint value.
	 *
	 *   font identifier: 16 bits
	 *     The index into rufl_font_list of a font providing a
	 *     substitution glyph for this codepoint or NOT_AVAILABLE.
	 *
	 * Note that, as the substitution table is sparse and may not be
	 * fully populated, it is necessary to verify that the Unicode
	 * codepoint matches the key being hashed and that the font
	 * identifier is not NOT_AVAILABLE. If either of these tests
	 * fail, no font provides a suitable glyph and the not available
	 * path should be taken.
	 */
	uint64_t *table;
	uint8_t bits_per_entry; /**< Bits per displacement bitmap entry */
	/** Displacement bitmap.
	 *
	 * The displacement values are stored in a bitmap of num_buckets
	 * fields each being bits_per_entry wide. Both values are computed
	 * at runtime.
	 */
	uint8_t displacement_map[];
};
/** Font substitution table */
static struct rufl_substitution_table *rufl_substitution_table;

/**
 * Round an unsigned 32bit value up to the next power of 2
 */
static uint32_t ceil2(uint32_t val)
{
	val--;
	val |= (val >> 1);
	val |= (val >> 2);
	val |= (val >> 4);
	val |= (val >> 8);
	val |= (val >> 16);
	val++;
	val += (val == 0);
	return val;
}

/**
 * Compute the number of bits needed to store a value
 */
static uint32_t bits_needed(uint32_t val)
{
	int32_t result = 0;

	if (val == 0)
		return 1;

	if ((val & (val - 1))) {
		/* Not a power of 2: round up */
		val = ceil2(val);
		/* Will need one fewer bit than we're about to count */
		result = -1;
	}

	while (val > 0) {
		result += 1;
		val >>= 1;
	}

	return (uint32_t) result;
}

/**
 * Perform one round of MurmurHash2
 */
static uint32_t mround(uint32_t val, uint32_t s)
{
	val *= 0x5db1e995;
	val ^= (val >> 24);
	val *= 0x5db1e995;
	val ^= (s * 0x5db1e995);

	return val;
}

/**
 * Perform the MurmurHash2 mixing step
 */
static uint32_t mmix(uint32_t val)
{
	val ^= (val >> 13);
	val *= 0x5db1e995;
	val ^= (val >> 15);

	return val;
}

/**
 * First-stage hash (i.e. g(x)) for substitution table.
 *
 * As we know that the input values are Unicode codepoints,
 * do some trivial bit manipulation, which has reasonable
 * distribution properties.
 */
static uint32_t hash1(uint32_t val)
{
	val ^= (val >> 7);
	val ^= (val << 3);
	val ^= (val >> 4);
	return val;
}

/**
 * Second-stage hash (i.e. f(d, x)) for substitution table.
 *
 * Apply MurmurHash2 to the value and displacement
 */
static uint32_t hash2(uint32_t val, uint32_t d)
{
	return mmix(mround(val, mround(d, 4)));
}

/**
 * Comparison function for table entries.
 *
 * We use this when sorting the intermediate table for CHD.
 */
static int table_chd_cmp(const void *a, const void *b)
{
	/* We're only interested in the CHD metadata here.
	 * (i.e. the computed value of g(x) and the bucket size) */
	const uint64_t aa = (*(const uint64_t *) a) & 0x3fe00000ffff0000llu;
	const uint64_t bb = (*(const uint64_t *) b) & 0x3fe00000ffff0000llu;

	if (aa > bb)
		return -1;
	else if (aa < bb)
		return 1;
	return 0;
}

/**
 * Test that all specified bits in a bit map are clear and set them if so.
 *
 * \param bitmap Bit map to inspect
 * \param idx    Table of indices to inspect
 * \param len    Number of entries in index table
 * \return True if all bits were clear. False otherwise.
 */
static bool test_and_set_bits(uint8_t *bitmap, const uint32_t *idx, size_t len)
{
	unsigned int i;
	bool result = true;

	/* Test if all specified bits are clear */
	for (i = 0; i != len; i++) {
		const uint32_t byte = (idx[i] >> 3);
		const uint32_t bit = (idx[i] & 0x7);

		result &= ((bitmap[byte] & (1 << bit)) == 0);
	}

	if (result) {
		/* They are, so set them */
		for (i = 0; i != len; i++) {
			const uint32_t byte = (idx[i] >> 3);
			const uint32_t bit = (idx[i] & 0x7);

			bitmap[byte] |= (1 << bit);
		}
	}

	return result;
}

/**
 * Create the final substitution table from the intermediate parts
 *
 * \param table               Substitution table
 * \param table_entries       Number of entries in table
 * \param buckets             Number of CHD buckets
 * \param range               Number of slots in final table
 * \param max_displacement    max(displacements)
 * \param displacements       Table of displacement values. One per bucket.
 * \param substitution_table  Location to receive result.
 */
static rufl_code create_substitution_table(uint64_t *table,
		size_t table_entries, uint32_t buckets, uint32_t range,
		uint32_t max_displacement, uint32_t *displacements,
		struct rufl_substitution_table **substitution_table)
{
	struct rufl_substitution_table *subst_table;
	size_t subst_table_size;
	unsigned int i;

#ifdef RUFL_SUBSTITUTION_TABLE_DEBUG
	LOG("max displacement of %u requires %u bits",
			max_displacement, bits_needed(max_displacement));
#endif

	subst_table_size = offsetof(struct rufl_substitution_table,
			displacement_map) +
			((buckets * bits_needed(max_displacement) + 7) >> 3);

	subst_table = calloc(subst_table_size, 1);
	if (!subst_table)
		return rufl_OUT_OF_MEMORY;

	/* We know there are at least table_entries in the table, but
	 * we should now resize it to the size of the target hashtable */
	subst_table->table = realloc(table, range * sizeof(*table));
	if (!subst_table->table) {
		free(subst_table);
		return rufl_OUT_OF_MEMORY;
	}
	/* Initialise unused slots */
	for (i = table_entries; i < range; i++) {
		subst_table->table[i] = NOT_AVAILABLE;
	}

	subst_table->num_buckets = buckets;
	subst_table->num_slots = range;
	subst_table->bits_per_entry = bits_needed(max_displacement);

	/* Fill in displacement map */
	//XXX: compress map using Fredriksson-Nikitin encoding?
	for (i = 0; i < buckets; i++) {
		uint32_t offset_bits = i * subst_table->bits_per_entry;
		uint32_t bits_to_write = subst_table->bits_per_entry;
		uint8_t *pwrite =
			&subst_table->displacement_map[offset_bits >> 3];

		offset_bits &= 7;

		while (bits_to_write > 0) {
			uint32_t space_available = (8 - offset_bits);
			uint32_t mask = 0, mask_idx;

			if (space_available > bits_to_write)
				space_available = bits_to_write;

			for (mask_idx = 0; mask_idx != space_available;
					mask_idx++) {
				mask <<= 1;
				mask |= 1;
			}

			*pwrite |= ((displacements[i] >>
				 (bits_to_write - space_available)) & mask) <<
				(8 - offset_bits - space_available);
			pwrite++;
			offset_bits = 0;
			bits_to_write -= space_available;
		}
	}

	/* Shuffle table data so the indices match the hash values */
	for (i = 0; i < table_entries; ) {
		uint32_t f, g;
		uint64_t tmp;

		if (subst_table->table[i] == NOT_AVAILABLE) {
			i++;
			continue;
		}

		g = ((subst_table->table[i] >> 53) & 0x1f0000) |
				((subst_table->table[i] >> 16) & 0xffff);
		f = hash2((subst_table->table[i] >> 32) & 0x1fffff,
				displacements[g]) & (range - 1);

		/* Exchange this entry with the one in the slot at f.*/
		if (f != i) {
			tmp = subst_table->table[f];
			subst_table->table[f] = subst_table->table[i];
			subst_table->table[i] = tmp;
		} else {
			/* Reconsider this slot unless it already
			 * had the correct entry */
			i++;
		}
	}
	/* Strip all the CHD metadata out of the final table */
	for (i = 0; i < range; i++)
		subst_table->table[i] &= 0x001fffff0000ffffllu;

	*substitution_table = subst_table;

	return rufl_OK;
}

/**
 * Compute a perfect hash to address the substitution table.
 *
 * We use the CHD algorithm to do this.
 * (https://doi.org/10.1007/978-3-642-04128-0_61 ;
 *  http://cmph.sourceforge.net/papers/esa09.pdf)
 *
 * A more recent alternative might be RecSplit
 * (https://arxiv.org/abs/1910.06416v2).
 *
 * \param table               Pre-filled table of raw substitution data
 * \param table_entries       Number of entries in the table
 * \param substitution_table  Location to receive result
 */
static rufl_code chd(uint64_t *table, size_t table_entries,
		struct rufl_substitution_table **substitution_table)
{
	/** Number of buckets assuming an average bucket size of 4 */
	const uint32_t buckets = ceil2((table_entries + 3) & ~3);
	/** Number of output hash slots assuming a load factor of 0.95 */
	const uint32_t range = ceil2((table_entries * 100)/95);
	uint32_t bucket_size, max_displacement = 0;
	unsigned int i;
	uint8_t *entries_per_bucket, *bitmap;
	uint32_t *displacements;
	rufl_code result = rufl_OK;

#ifdef RUFL_SUBSTITUTION_TABLE_DEBUG
	LOG("hashing %zu entries into %u buckets with range %u",
			table_entries, buckets, range);
#endif

	entries_per_bucket = calloc(buckets, sizeof(*entries_per_bucket));
	if (!entries_per_bucket)
		return rufl_OUT_OF_MEMORY;

	bitmap = calloc(range >> 3, 1);
	if (!bitmap) {
		free(entries_per_bucket);
		return rufl_OUT_OF_MEMORY;
	}

	displacements = calloc(buckets, sizeof(*displacements));
	if (!displacements) {
		free(bitmap);
		free(entries_per_bucket);
		return rufl_OUT_OF_MEMORY;
	}

	/* Compute g(x) for each entry, placing them into buckets */
	for (i = 0; i < table_entries; i++) {
		uint32_t g = hash1((table[i] >> 32) & 0x1fffff) & (buckets - 1);

		/* Insert hash into entry (it's 21 bits at most, so
		 * needs splitting between bits 16-31 and 53-57 of
		 * the entry) */
		table[i] |= ((g & 0xffff) << 16);
		table[i] |= ((uint64_t)(g & 0x1f0000) << 53);

		entries_per_bucket[g]++;
	}

	/* Inject bucket size into entries */
	for (i = 0; i < table_entries; i++) {
		uint32_t g = ((table[i] >> 53) & 0x1f0000) |
			((table[i] >> 16) & 0xffff);

		/* With a target bucket size of 4, do not expect
		 * >= twice that number of entries in the largest
		 * bucket. If there are, the hash function needs
		 * work (we allocate 4 bits for the bucket size,
		 * so should have sufficient headroom). */
		if (entries_per_bucket[g] >= 8)
			LOG("unexpectedly large bucket %u",
					entries_per_bucket[g]);

		/* Stash bucket size into bits 58-61 of the entry */
		table[i] |= ((uint64_t)entries_per_bucket[g] << 58);
	}

	/* Bits 62-63 of table entries are currently unused */

	free(entries_per_bucket);

	/* Sort entries in descending bucket size order */
	qsort(table, table_entries, sizeof(*table), table_chd_cmp);

	/* Compute f(x) for each bucket, finding a unique mapping */
	for (i = 0; i < table_entries; i += bucket_size) {
		const uint32_t g = ((table[i] >> 53) & 0x1f0000) |
			((table[i] >> 16) & 0xffff);
		uint32_t hashes[8], num_hashes;
		uint32_t d = 0;

		bucket_size = ((table[i] >> 58) & 0xf);

		do {
			uint32_t j, k;

			d++;
			num_hashes = 0;

			for (j = 0; j != bucket_size; j++) {
				uint32_t f = hash2(
					(table[i+j] >> 32) & 0x1fffff, d) &
					(range - 1);
				for (k = 0; k < num_hashes; k++) {
					if (f == hashes[k])
						break;
				}
				if (k == num_hashes) {
					hashes[num_hashes] = f;
					num_hashes++;
				}
			}
		} while (num_hashes != bucket_size || !test_and_set_bits(
				bitmap, hashes, num_hashes));

		displacements[g] = d;
		if (d > max_displacement)
			max_displacement = d;
	}

	free(bitmap);

	result = create_substitution_table(table, table_entries,
			buckets, range, max_displacement, displacements,
			substitution_table);
	free(displacements);

	return result;
}

/**
 * Populate the substitution map for a given block
 */
static void fill_map_for_block(const struct rufl_character_set **charsets,
		uint32_t block, uint16_t map_for_block[256])
{
	unsigned int i, u;

	for (i = 0; i != rufl_font_list_entries; i++) {
		if (!charsets[i])
			continue;

		if (charsets[i]->index[block] == BLOCK_FULL) {
			for (u = 0; u != 256; u++)
				if (map_for_block[u] == NOT_AVAILABLE)
					map_for_block[u] = i;
		} else if (charsets[i]->index[block] != BLOCK_EMPTY) {
			const uint8_t *blk = charsets[i]->block[
					charsets[i]->index[block]];
			for (u = 0; u != 256; u++) {
				if (map_for_block[u] == NOT_AVAILABLE &&
						(blk[(u>>3)] & (1 << (u&7)))) {
					map_for_block[u] = i;
				}
			}
		}
	}
}


/**
 * Construct the font substitution table.
 */

rufl_code rufl_substitution_table_init(void)
{
	unsigned int i;
	unsigned int plane, block;
	unsigned int u;
	const struct rufl_character_set **charsets;
	uint64_t *table;
	size_t table_size;
	size_t table_entries;
	rufl_code result;

	charsets = malloc(rufl_font_list_entries * sizeof(*charsets));
	if (!charsets) {
		LOG("malloc(%zu) failed",
				rufl_font_list_entries * sizeof(*charsets));
		return rufl_OUT_OF_MEMORY;
	}

	table = malloc(1024 * sizeof(*table));
	if (!table) {
		LOG("malloc(%zu) failed", 1024 * sizeof(*table));
		free(charsets);
		return rufl_OUT_OF_MEMORY;
	}
	table_size = 1024;
	table_entries = 0;

	for (plane = 0; plane < 17; plane++) {
		const struct rufl_character_set *charset;
		unsigned int num_charsets = 0;

		/* Find fonts that have charsets for this plane */
		for (i = 0; i != rufl_font_list_entries; i++) {
			charset = rufl_font_list[i].charset;
			if (!charset) {
				charsets[i] = NULL;
				continue;
			}

			while (PLANE_ID(charset->metadata) != plane &&
					EXTENSION_FOLLOWS(charset->metadata)) {
				charset = (void *)(((uint8_t *)charset) +
						PLANE_SIZE(charset->metadata));
			}
			if (PLANE_ID(charset->metadata) != plane)
				charset = NULL;
			charsets[i] = charset;
			num_charsets++;
		}
		if (num_charsets == 0)
			continue;

		/* Process each block, finding fonts that have glyphs */
		for (block = 0; block != 256; block++) {
			uint16_t map_for_block[256];
			memset(map_for_block, 0xff, 512);

			fill_map_for_block(charsets, block, map_for_block);

			/* Merge block map into table */
			for (i = 0; i != 256; i++) {
				if (map_for_block[i] == NOT_AVAILABLE)
					continue;

				u = (plane << 16) | (block << 8) | i;
				table[table_entries] = ((uint64_t) u << 32) |
							map_for_block[i];
				if (++table_entries == table_size) {
					uint64_t *tmp = realloc(table,
							2 * table_size *
							sizeof(*table));
					if (!tmp) {
						LOG("realloc(%zu) failed",
								2 * table_size *
								sizeof(*table));
						free(table);
						return rufl_OUT_OF_MEMORY;
					}

					table = tmp;
					table_size *= 2;
				}
			}
		}
	}

	/* Build hash from table */
	result = chd(table, table_entries, &rufl_substitution_table);

#ifdef RUFL_SUBSTITUTION_TABLE_DEBUG
	LOG("table size(%zu) entries %zu buckets(%u@%ubpe => %u)",
			rufl_substitution_table->num_slots * sizeof(*table),
			table_entries,
			rufl_substitution_table->num_buckets,
			rufl_substitution_table->bits_per_entry,
			(rufl_substitution_table->num_buckets *
			 rufl_substitution_table->bits_per_entry + 7) >> 3);
#endif

	free(charsets);

	return result;
}

/**
 * Destroy the substitution table and clean up its resources
 */

void rufl_substitution_table_fini(void)
{
	free(rufl_substitution_table->table);
	free(rufl_substitution_table);
	rufl_substitution_table = NULL;
}

/**
 * Look up a Unicode codepoint in the substitution table
 */

unsigned int rufl_substitution_table_lookup(uint32_t u)
{
	uint32_t displacement = 0;
	uint32_t f, g = hash1(u & 0x1ffffff) &
			(rufl_substitution_table->num_buckets - 1);
	uint32_t bits_to_read = rufl_substitution_table->bits_per_entry;
	uint32_t offset_bits = g * bits_to_read;
	const uint8_t *pread =
		&rufl_substitution_table->displacement_map[offset_bits >> 3];

	offset_bits &= 7;

	while (bits_to_read > 0) {
		uint32_t space_available = (8 - offset_bits);
		if (space_available > bits_to_read)
			space_available = bits_to_read;

		displacement <<= space_available;
		displacement |= (*pread & (0xff >> offset_bits)) >>
				(8 - space_available - offset_bits);

		offset_bits += space_available;
		if (offset_bits >= 8) {
			pread++;
			offset_bits = 0;
		}
		bits_to_read -= space_available;
	}

	f = hash2((u & 0x1fffff), displacement) &
			(rufl_substitution_table->num_slots - 1);

	if ((rufl_substitution_table->table[f] & 0xffff) != NOT_AVAILABLE &&
		((rufl_substitution_table->table[f] >> 32) & 0x1fffff) == u)
		return rufl_substitution_table->table[f] & 0xffff;

	return NOT_AVAILABLE;
}

static int table_dump_cmp(const void *a, const void *b)
{
	const uint64_t aa = (*(const uint64_t *) a) & 0x001fffff00000000llu;
	const uint64_t bb = (*(const uint64_t *) b) & 0x001fffff00000000llu;

	if (aa > bb)
		return 1;
	else if (aa < bb)
		return -1;
	return 0;
}

/**
 * Dump a representation of the substitution table to stdout.
 */

void rufl_substitution_table_dump(void)
{
	unsigned int font;
	unsigned int u, t;
	uint64_t *table;

	table = malloc(rufl_substitution_table->num_slots * sizeof(*table));
	if (table == NULL)
		return;

	memcpy(table, rufl_substitution_table->table,
			rufl_substitution_table->num_slots * sizeof(*table));

	qsort(table, rufl_substitution_table->num_slots, sizeof(*table),
			table_dump_cmp);

	u = 0;
	while (u < rufl_substitution_table->num_slots) {
		t = u;
		font = table[t] & 0xffff;
		while (u < rufl_substitution_table->num_slots &&
				font == (table[u] & 0xffff) &&
				((u == t) ||
				 (((table[u - 1] >> 32) & 0x1fffff) ==
				  (((table[u] >> 32) & 0x1fffff) - 1))))
			u++;
		if (font != NOT_AVAILABLE)
			printf("  %llx-%llx => %u \"%s\"\n",
					(table[t] >> 32) & 0x1fffff,
					(table[u - 1] >> 32) & 0x1fffff,
					font, rufl_font_list[font].identifier);
	}

	free(table);
}
