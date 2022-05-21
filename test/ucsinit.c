#include <stdio.h>

#include "rufl.h"

#include "harness.h"
#include "testutils.h"

int main(int argc, const char **argv)
{
	int width, x;
	size_t offset;
	int32_t xkern, ykern, italic, ascent, descent, xheight, cap_height;
	int8_t uline_position;
	uint8_t uline_thickness;
	os_box bbox;

	UNUSED(argc);
	UNUSED(argv);

	rufl_test_harness_init(380, true, true);

	assert(rufl_OK == rufl_init());
	assert(NULL == rufl_fm_error);
	assert(3 == rufl_family_list_entries);
	assert(NULL != rufl_family_menu);

	assert(rufl_OK == rufl_font_metrics("Corpus", rufl_WEIGHT_500,
			&bbox, &xkern, &ykern, &italic,
			&ascent, &descent, &xheight, &cap_height,
			&uline_position, &uline_thickness));
	assert(0 == bbox.x0);
	assert(2 == bbox.x1);
	assert(0 == bbox.y0);
	assert(2 == bbox.y1);
	assert(0 == xkern);
	assert(0 == ykern);
	assert(0 == italic);
	assert(0 == ascent);
	assert(0 == descent);
	assert((bbox.y1 - bbox.y0) == cap_height);
	assert((cap_height / 2) == xheight);
	assert(0 == uline_position);
	assert(0 == uline_thickness);

	assert(rufl_OK == rufl_width("Corpus", rufl_WEIGHT_500, 10,
			(const uint8_t *) "!\xc2\xa0", 3, &width));
	assert(2 == width);

	assert(rufl_OK == rufl_x_to_offset("Homerton", rufl_WEIGHT_500, 10,
			(const uint8_t *) "!\xc2\xa0", 3, 1,
			&offset, &x));
	assert(1 == offset);
	assert(1 == x);

	assert(rufl_OK == rufl_split("Trinity", rufl_WEIGHT_500, 10,
			(const uint8_t *) "!\xc2\xa0", 3, 1,
			&offset, &x));
	assert(1 == offset);
	assert(1 == x);

	rufl_dump_state(true);

	rufl_quit();

	printf("PASS\n");

	return 0;
}
