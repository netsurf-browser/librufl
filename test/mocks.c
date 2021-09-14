#include <assert.h>
#include <string.h>

#include <oslib/font.h>
#include <oslib/hourglass.h>
#include <oslib/os.h>
#include <oslib/osfscontrol.h>
#include <oslib/taskwindow.h>
#include <oslib/wimp.h>
#include <oslib/wimpreadsysinfo.h>

#include "harness-priv.h"

static os_error font_no_font = { error_FONT_NO_FONT, "Undefined font handle" };
static os_error font_bad_font_number = {
	error_FONT_BAD_FONT_NUMBER,
	"Font handle out of range"
};
static os_error font_not_found = { error_FONT_NOT_FOUND, "Font not found" };
static os_error font_encoding_not_found = {
	error_FONT_ENCODING_NOT_FOUND,
	"Encoding not found"
};
static os_error font_no_handles = {
	error_FONT_NO_HANDLES,
	"No more font handles"
};
static os_error font_reserved = {
	error_FONT_RESERVED,
	"Reserved fields must be zero"
};
static os_error buff_overflow = { error_BUFF_OVERFLOW, "Buffer overflow" };
static os_error bad_parameters = { error_BAD_PARAMETERS, "Bad parameters" };
static os_error no_such_swi = { error_NO_SUCH_SWI, "SWI not known" };
static os_error unimplemented = { error_UNIMPLEMENTED, "Not implemented" };

/****************************************************************************/

os_error *xfont_cache_addr (int *version, int *cache_size, int *cache_used)
{
	if (version != NULL)
		*version = h->fm_version;
	if (cache_size != NULL)
		*cache_size = 512 * 1024;
	if (cache_used != NULL)
		*cache_used = 0;

	return NULL;
}

os_error *xfont_find_font (char const *font_name, int xsize, int ysize,
		int xres, int yres, font_f *font, int *xres_out, int *yres_out)
{
	char name[80], encoding[80];
	const char *slash;
	size_t ni, ei;
	int fh;

	/* Default xres and yres */
	if (xres <= 0)
		xres = 90;
	if (yres <= 0)
		yres = 90;

	/* Parse font name */
	slash = strchr(font_name, '\\');
	if (slash == NULL) {
		/* Bare name: assume Latin1 encoding */
		strncpy(name, font_name, sizeof(name));
		name[sizeof(name)-1] = '\0';
		strcpy(encoding, "Latin1");
	} else {
		/* Identifier: extract encoding */
		strncpy(name, font_name, slash - font_name);
		name[slash-font_name] = '\0';
		assert(slash[1] == 'E');
		strncpy(encoding, slash + 2, sizeof(encoding));
		encoding[sizeof(encoding)-1] = '\0';
	}

	/* Determine if we know about this font name */
	for (ni = 0; ni < h->n_font_names; ni++) {
		if (strcmp(h->font_names[ni], name) == 0)
			break;
	}
	if (ni == h->n_font_names) {
		return &font_not_found;
	}

	/* Determine if we know about this encoding */
	for (ei = 0; ei < h->n_encodings; ei++) {
		if (strcmp(h->encodings[ei], encoding) == 0)
			break;
	}
	if (ei == h->n_encodings) {
		return &font_encoding_not_found;
	}

	/* Find existing font handle (0 is forbidden) */
	for (fh = 1; fh < 256; fh++) {
		if (h->fonts[fh].refcnt > 0 && h->fonts[fh].name == ni &&
				h->fonts[fh].encoding == ei &&
				h->fonts[fh].xsize == xsize &&
				h->fonts[fh].ysize == ysize &&
				h->fonts[fh].xres == xres &&
				h->fonts[fh].yres == yres)
			break;
	}
	if (fh == 256) {
		/* No existing font found: allocate new one */
		for (fh = 1; fh < 256; fh++) {
			if (h->fonts[fh].refcnt == 0)
				break;
		}
		if (fh == 256) {
			return &font_no_handles;
		}

		h->fonts[fh].name = ni;
		h->fonts[fh].encoding = ei;
		h->fonts[fh].xsize = xsize;
		h->fonts[fh].ysize = ysize;
		h->fonts[fh].xres = xres;
		h->fonts[fh].yres = yres;
	}

	/* Bump refcnt */
	h->fonts[fh].refcnt++;

	/* Set current font */
	h->current_font = fh;

	if (font != NULL)
		*font = (font_f) fh;
	if (xres_out != NULL)
		*xres_out = xres;
	if (yres_out != NULL)
		*yres_out = yres;

	return NULL;
}

os_error *xfont_lose_font (font_f font)
{
	if (font != 0 && h->fonts[font].refcnt > 0)
		h->fonts[font].refcnt--;

	return NULL;
}

os_error *xfont_read_info (font_f font, int *x0, int *y0, int *x1, int *y1)
{
	if (font == 0)
		return &font_bad_font_number;
	if (h->fonts[font].refcnt == 0)
		return &font_no_font;

	/* Cheat: just scale point size to OS units */
	if (x0 != NULL)
		*x0 = 0;
	if (y0 != NULL)
		*y0 = 0;
	if (x1 != NULL)
		*x1 = ((h->fonts[font].xsize >> 4) * 72) / 180;
	if (y1 != NULL)
		*y1 = ((h->fonts[font].ysize >> 4) * 72) / 180;

	return NULL;
}

os_error *xfont_read_encoding_filename (font_f font, char *buffer, int size,
		char **end)
{
	if (font == 0)
		return &font_bad_font_number;
	if (h->fonts[font].refcnt == 0)
		return &font_no_font;

	//XXX:
	(void) buffer;
	(void) size;
	(void) end;

	return &unimplemented;
}

os_error *xfont_list_fonts (byte *buffer1, font_list_context context,
		int size1, byte *buffer2, int size2, char const *tick_font,
		font_list_context *context_out, int *used1, int *used2)
{
	const char **values;
	size_t n_values;
	size_t index = (context & 0xffff);

	if ((context & font_RETURN_FONT_MENU) &&
			(context & ~(font_USE_LINEFEED |
				     font_RETURN_FONT_MENU |
				     font_ALLOW_SYSTEM_FONT |
				     font_GIVEN_TICK | 0x400000)) >> 16)
		return &bad_parameters;
	if (!(context & font_RETURN_FONT_MENU) &&
			(context & ~(font_RETURN_FONT_NAME |
				     font_RETURN_LOCAL_FONT_NAME |
				     font_USE_LINEFEED | 0x400000)) >> 16)
		return &bad_parameters;
	if (context & font_RETURN_FONT_MENU)
		return &unimplemented;

	if (context & 0x400000) {
		values = h->encodings;
		n_values = h->n_encodings;
	} else {
		values = h->font_names;
		n_values = h->n_font_names;
	}

	if (index < n_values) {
		int len = (int) strlen(values[index]) + 1;
		if (context & font_RETURN_FONT_NAME) {
			if (buffer1 != NULL && size1 < len)
				return &buff_overflow;
			if (buffer1 != NULL)
				strcpy((char *) buffer1, values[index]);
			if (used1 != NULL)
				*used1 = len;
		}
		if (context & font_RETURN_LOCAL_FONT_NAME) {
			if (buffer2 != NULL && size2 < len)
				return &buff_overflow;
			if (buffer2 != NULL)
				strcpy((char *) buffer2, values[index]);
			if (used2 != NULL)
				*used2 = len;
		}
		index++;
	} else {
		index = -1;
	}

	if (context_out != NULL)
		*context_out = (font_list_context) index;

	(void) tick_font;

	return NULL;
}

os_error *xfont_set_font (font_f font)
{
	if (font == 0)
		return &font_bad_font_number;
	if (h->fonts[font].refcnt == 0)
		return &font_no_font;

	h->current_font = font;

	return NULL;
}

os_error *xfont_paint (font_f font, char const *string,
		font_string_flags flags, int xpos, int ypos,
		font_paint_block const *block, os_trfm const *trfm, int length)
{
	if (!(flags & font_GIVEN_FONT) || font == 0)
		font = h->current_font;
	if (font == 0 || h->fonts[font].refcnt == 0)
		return &font_no_font;

	if (flags & font_GIVEN_FONT)
		h->current_font = font;

	//XXX:
	//XXX: also, pay attention to redirection to buffer
	(void) string;
	(void) xpos;
	(void) ypos;
	(void) block;
	(void) trfm;
	(void) length;

	return &unimplemented;
}

os_error *xfont_scan_string (font_f font, char const *s,
		font_string_flags flags, int x, int y, font_scan_block *block,
		os_trfm const *trfm, int length, char **split_point,
		int *x_out, int *y_out, int *num_split_chars)
{
	size_t advance = 1;
	int width = 0;

	if (!(flags & font_GIVEN_FONT) || font == 0)
		font = h->current_font;
	if (font == 0 || h->fonts[font].refcnt == 0)
		return &font_no_font;

	if ((flags & font_GIVEN_BLOCK) && block == NULL)
		return &bad_parameters;
	if ((flags & font_RETURN_BBOX) && !(flags & font_GIVEN_BLOCK))
		return &bad_parameters;
	if ((flags & font_GIVEN_BLOCK) && (block->space.x != 0 ||
			block->space.y != 0 ||
			block->letter.x != 0 ||
			block->letter.y != 0 ||
			block->split_char != -1))
		return &unimplemented;

	if ((flags & font_GIVEN32_BIT) && (flags & font_GIVEN16_BIT))
		return &bad_parameters;

	if (!(flags & font_GIVEN_LENGTH))
		length = 0x7ffffffc;

	if (flags & font_GIVEN32_BIT)
		advance = 4;
	else if (flags & font_GIVEN16_BIT)
		advance = 2;

	/* Consume up to length bytes of input */
	while (length > 0) {
		uint32_t c = 0, i;
		for (i = 0; i < advance; i++) {
			c |= s[i] << (advance - i - 1);
		}
		s += advance;
		length -= advance;

		/* Regardless of length, stop on terminator */
		if (c == 0 || c == 10 || c == 13)
			break;

		/* Just scale font size to millipoints and add on the width */
		width += ((h->fonts[font].xsize * 1000) >> 4);
		//XXX: how is negative x meant to work?
		if (x > 0 && width > x)
			break;
	}

	if (flags & font_RETURN_BBOX) {
		block->bbox.x0 = 0;
		block->bbox.y0 = 0;
		block->bbox.x1 = width;
		block->bbox.y1 = (h->fonts[font].ysize * 1000) >> 4;
	}

	if (x_out != NULL)
		*x_out = width;
	if (y_out != NULL)
		*y_out = (h->fonts[font].ysize * 1000) >> 4;

	(void) y;
	(void) trfm;
	(void) split_point;
	(void) num_split_chars;

	return NULL;
}

os_error *xfont_switch_output_to_buffer (font_output_flags flags,
		byte *buffer, char **end)
{
	if ((intptr_t) buffer <= 0 && flags != 0)
		return &font_reserved;
	if (flags & ~(font_NO_OUTPUT | font_ADD_HINTS | font_ERROR_IF_BITMAP))
		return &font_reserved;

	if (end)
		*end = h->buffer;

	if ((intptr_t) buffer != -1) {
		h->buffer = (char *) buffer;
		h->buffer_flags = flags;
	}

	return NULL;
}

os_error *xfont_enumerate_characters (font_f font, int character,
		int *next_character, int *internal_character_code)
{
	static int extchars[] = {     0x20, 0x21, 0x30, 0x31, 0x40, -1 };
	static int intchars[] = { -1,    1,    2,   -1,    3,    4 };
	int index;

	if (!h->fm_ucs)
		return &no_such_swi;

	if (font == 0)
		font = h->current_font;
	if (font == 0)
		return &font_bad_font_number;
	if (h->fonts[font].refcnt == 0)
		return &font_no_font;

	/* First character */
	if (character == 0) {
		index = 0;
		if (h->fm_broken_fec)
			index = 2;
	} else {
		for (index = 0; index < 6; index++) {
			if (extchars[index] == character)
				break;
		}
		index++;
	}

	if (next_character != NULL)
		*next_character = extchars[index];
	if (internal_character_code != NULL)
		*internal_character_code = intchars[index];

	return NULL;
}

/****************************************************************************/

os_error *xhourglass_on (void)
{
	return &unimplemented;
}

os_error *xhourglass_off (void)
{
	return &unimplemented;
}

os_error *xhourglass_percentage (int percent)
{
	(void) percent;

	return &unimplemented;
}

os_error *xhourglass_leds (bits eor_mask, bits and_mask, bits *old_leds)
{
	(void) eor_mask;
	(void) and_mask;
	(void) old_leds;

	return &unimplemented;
}

os_error *xhourglass_colours (os_colour sand, os_colour glass,
		os_colour *old_sand, os_colour *old_glass)
{
	(void) sand;
	(void) glass;
	(void) old_sand;
	(void) old_glass;

	return &unimplemented;
}

/****************************************************************************/

os_error *xos_read_monotonic_time (os_t *t)
{
	(void) t;

	return &unimplemented;
}

os_error *xos_read_mode_variable (os_mode mode, os_mode_var var, int *var_val,
		bits *psr)
{
	(void) mode;
	(void) var;
	(void) var_val;
	(void) psr;

	return &unimplemented;
}

/****************************************************************************/

os_error *xosfscontrol_canonicalise_path (char const *path_name, char *buffer,
		char const *var, char const *path, int size, int *spare)
{
	const char *prefix = "Resources:$.Fonts.";
	size_t len = strlen(path_name) + strlen(prefix) + 1;

	if (strcmp(var, "Font$Path") != 0 || path != NULL)
		return &unimplemented;

	if (buffer == NULL && size != 0)
		return &bad_parameters;

	if (buffer != NULL && size < (int) len)
		return &buff_overflow;

	if (buffer != NULL) {
		strcpy(buffer, prefix);
		strcpy(buffer + strlen(prefix), path_name);
	}

	if (spare != NULL)
		*spare = size - len;

	return NULL;
}

/****************************************************************************/

os_error *xtaskwindowtaskinfo_window_task (osbool *window_task)
{
	(void) window_task;

	return &unimplemented;
}

/****************************************************************************/

os_error *xwimp_create_window (wimp_window const *window, wimp_w *w)
{
	(void) window;
	(void) w;

	return &unimplemented;
}

os_error *xwimp_delete_window (wimp_w w)
{
	(void) w;

	return &unimplemented;
}

os_error *xwimp_get_window_state (wimp_window_state *state)
{
	(void) state;

	return &unimplemented;
}

os_error *xwimp_open_window (wimp_open *open)
{
	(void) open;

	return &unimplemented;
}

os_error *xwimp_set_icon_state (wimp_w w, wimp_i i, wimp_icon_flags eor_bits,
		wimp_icon_flags clear_bits)
{
	(void) w;
	(void) i;
	(void) eor_bits;
	(void) clear_bits;

	return &unimplemented;
}

os_error *xwimp_resize_icon (wimp_w w, wimp_i i, int x0, int y0, int x1, int y1)
{
	(void) w;
	(void) i;
	(void) x0;
	(void) y0;
	(void) x1;
	(void) y1;

	return &unimplemented;
}

os_error *xwimp_poll (wimp_poll_flags mask, wimp_block *block, int *pollword,
		wimp_event_no *event)
{
	(void) mask;
	(void) block;
	(void) pollword;
	(void) event;

	return &unimplemented;
}

/****************************************************************************/

os_error *xwimpreadsysinfo_task (wimp_t *task, wimp_version_no *version)
{
	(void) task;
	(void) version;

	return &unimplemented;
}
