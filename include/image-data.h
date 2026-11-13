#ifndef MAKO_IMAGE_DATA_H
#define MAKO_IMAGE_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Compute the number of bytes of pixel data an image-data notification
// payload describes: every row but the last occupies rowstride bytes, the
// last row is unpadded and holds width pixels of
// ceil(channels * bits_per_sample / 8) bytes each.
//
// All fields come straight from the D-Bus message and may contain garbage.
// Returns false when the geometry is malformed, when the length overflows
// size_t, or when fewer than the declared number of bytes were transmitted.
bool image_data_size(int32_t width, int32_t height, int32_t rowstride,
		int32_t channels, int32_t bits_per_sample, size_t transmitted,
		size_t *out_len);

#endif
