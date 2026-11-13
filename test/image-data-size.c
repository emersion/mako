#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "image-data.h"

static int failures = 0;

static void check(const char *name, int32_t width, int32_t height,
		int32_t rowstride, int32_t channels, int32_t bits_per_sample,
		size_t transmitted, bool expect_ok, size_t expect_len) {
	size_t len = 0;
	bool ok = image_data_size(width, height, rowstride, channels,
		bits_per_sample, transmitted, &len);
	if (ok != expect_ok || (ok && len != expect_len)) {
		fprintf(stderr, "FAIL %s: image_data_size(%d, %d, %d, %d, %d, %zu)"
			" -> %s len=%zu (expected %s len=%zu)\n",
			name, width, height, rowstride, channels, bits_per_sample,
			transmitted, ok ? "ok" : "reject", len,
			expect_ok ? "ok" : "reject", expect_len);
		failures++;
	}
}

int main(void) {
	// Ordinary RGBA pixmap: every row but the last is padded to rowstride,
	// so the useful length is smaller than the transmitted buffer.
	check("rgb", 100, 5, 400, 4, 8, 2000, true, 4 * 400 + 100 * 4);
	// Transmitted data may include the last row's padding too.
	check("rgb-padded", 100, 5, 400, 4, 8, 5 * 400, true, 2000);
	// Single pixel.
	check("1px", 1, 1, 4, 4, 8, 4, true, 4);
	// Sub-byte samples round up to a whole byte per pixel.
	check("sub-byte", 10, 3, 2, 1, 1, 16, true, 2 * 2 + 10);
	// A crafted geometry makes the useful length wrap around to a tiny
	// value, so the tiny buffer is then read at the huge declared
	// geometry (https://github.com/emersion/mako/issues/664).
	check("wrapped-length", 100, 5, 1073741725, 4, 8, 4, false, 0);
	// height = 0 must not underflow the (height - 1) term.
	check("zero-height", 10, 0, 40, 4, 8, 400, false, 0);
	// Non-positive fields are malformed.
	check("neg-height", 10, -5, 40, 4, 8, 400, false, 0);
	check("zero-width", 0, 5, 40, 4, 8, 200, false, 0);
	check("neg-rowstride", 10, 5, -40, 4, 8, 200, false, 0);
	check("zero-channels", 10, 5, 40, 0, 8, 200, false, 0);
	check("neg-bits", 10, 5, 40, 4, -8, 200, false, 0);
	// The transmitted byte count must cover the declared geometry.
	check("truncated", 10, 10, 40, 4, 8, 399, false, 0);
	check("exact", 10, 10, 40, 4, 8, 400, true, 400);
	// Products overflowing even a 64-bit size_t are rejected.
	check("overflow", INT32_MAX, 1, 4, INT32_MAX, INT32_MAX, SIZE_MAX, false, 0);

	if (failures > 0) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	return 0;
}
