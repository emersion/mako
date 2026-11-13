#include "image-data.h"

bool image_data_size(int32_t width, int32_t height, int32_t rowstride,
		int32_t channels, int32_t bits_per_sample, size_t transmitted,
		size_t *out_len) {
	if (width <= 0 || height <= 0 || rowstride <= 0 || channels <= 0 ||
			bits_per_sample <= 0) {
		return false;
	}

	// Use checked arithmetic: the fields are sender-controlled, so a
	// crafted geometry must not be able to wrap the length around and
	// trick us into under-allocating the pixel buffer.
	size_t bits, last_row, len;
	if (__builtin_mul_overflow((size_t)channels, (size_t)bits_per_sample, &bits) ||
			__builtin_add_overflow(bits, (size_t)7, &bits) ||
			__builtin_mul_overflow((size_t)width, bits / 8, &last_row) ||
			__builtin_mul_overflow((size_t)height - 1, (size_t)rowstride, &len) ||
			__builtin_add_overflow(len, last_row, &len) ||
			len > transmitted) {
		return false;
	}

	*out_len = len;
	return true;
}
