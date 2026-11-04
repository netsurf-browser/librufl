#ifndef rufl_test_harness_h_
#define rufl_test_harness_h_

typedef struct {
	struct {
		const char *name;
		const char *encoding;
		int xsize;
		int ysize;
		int xres;
		int yres;
	} font;
	size_t len;
	int xpos;
	int ypos;
	bool ucs4;
	bool trfm;
	unsigned char data[];
} rufl_test_harness_paint_call;

void rufl_test_harness_init(int fm_version, bool fm_ucs, bool preload);
void rufl_test_harness_register_font(const char *name);
void rufl_test_harness_register_encoding(const char *encoding);
void rufl_test_harness_set_font_encoding(const char *fontname,
		const char *encoding, const char *path);
void rufl_test_harness_set_font_glyph_map(const char *fontname,
		const char *path);

const rufl_test_harness_paint_call *rufl_test_harness_paint_enumerate(
		const rufl_test_harness_paint_call *current);
void rufl_test_harness_assert_paint_call(
		const rufl_test_harness_paint_call *call,
		const rufl_test_harness_paint_call *exp,
		const char *text);
void rufl_test_harness_assert_paint_calls(const char *fontname, ...);
void rufl_test_harness_discard_paint_calls(void);

#endif
