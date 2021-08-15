#include <oslib/font.h>
#include <oslib/hourglass.h>
#include <oslib/os.h>
#include <oslib/osfscontrol.h>
#include <oslib/taskwindow.h>
#include <oslib/wimp.h>
#include <oslib/wimpreadsysinfo.h>

static os_error unimplemented = { error_UNIMPLEMENTED, "Not implemented" };

/****************************************************************************/

os_error *xfont_cache_addr (int *version, int *cache_size, int *cache_used)
{
	(void) version;
	(void) cache_size;
	(void) cache_used;

	return &unimplemented;
}

os_error *xfont_find_font (char const *font_name, int xsize, int ysize,
		int xres, int yres, font_f *font, int *xres_out, int *yres_out)
{
	(void) font_name;
	(void) xsize;
	(void) ysize;
	(void) xres;
	(void) yres;
	(void) font;
	(void) xres_out;
	(void) yres_out;

	return &unimplemented;
}

os_error *xfont_lose_font (font_f font)
{
	(void) font;

	return &unimplemented;
}

os_error *xfont_read_info (font_f font, int *x0, int *y0, int *x1, int *y1)
{
	(void) font;
	(void) x0;
	(void) y0;
	(void) x1;
	(void) y1;

	return &unimplemented;
}

os_error *xfont_read_encoding_filename (font_f font, char *buffer, int size,
		char **end)
{
	(void) font;
	(void) buffer;
	(void) size;
	(void) end;

	return &unimplemented;
}

os_error *xfont_list_fonts (byte *buffer1, font_list_context context,
		int size1, byte *buffer2, int size2, char const *tick_font,
		font_list_context *context_out, int *used1, int *used2)
{
	(void) buffer1;
	(void) context;
	(void) size1;
	(void) buffer2;
	(void) size2;
	(void) tick_font;
	(void) context_out;
	(void) used1;
	(void) used2;

	return &unimplemented;
}

os_error *xfont_set_font (font_f font)
{
	(void) font;

	return &unimplemented;
}

os_error *xfont_paint (font_f font, char const *string,
		font_string_flags flags, int xpos, int ypos,
		font_paint_block const *block, os_trfm const *trfm, int length)
{
	(void) font;
	(void) string;
	(void) flags;
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
	(void) font;
	(void) s;
	(void) flags;
	(void) x;
	(void) y;
	(void) block;
	(void) trfm;
	(void) length;
	(void) split_point;
	(void) x_out;
	(void) y_out;
	(void) num_split_chars;

	return &unimplemented;
}

os_error *xfont_switch_output_to_buffer (font_output_flags flags,
		byte *buffer, char **end)
{
	(void) flags;
	(void) buffer;
	(void) end;

	return &unimplemented;
}

os_error *xfont_enumerate_characters (font_f font, int character,
		int *next_character, int *internal_character_code)
{
	(void) font;
	(void) character;
	(void) next_character;
	(void) internal_character_code;

	return &unimplemented;
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
	(void) path_name;
	(void) buffer;
	(void) var;
	(void) path;
	(void) size;
	(void) spare;

	return &unimplemented;
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
