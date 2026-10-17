/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

#include "rufl_internal.h"


/**
 * Given desired style and proposed substitute font, find related font with
   closest style.
 */
rufl_code rufl_optimise_style(unsigned int *font, rufl_style font_style,
		unsigned int u)
{
	unsigned int font1, font2;
	const char *font1_family;
	struct rufl_character_set *charset;
	rufl_code code;

	font1 = *font;

	if (font1 == NOT_AVAILABLE)
		return rufl_OK;

	font1_family = rufl_family_list[rufl_font_list[font1].family];
	code = rufl_find_font_family(font1_family, font_style, &font2, NULL,
			&charset);
	if (code != rufl_OK)
		return code;
	if (font2 != font1 && charset &&
			rufl_character_set_test(charset, u))
		*font = font2;

	return rufl_OK;
}
