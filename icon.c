#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <cairo/cairo.h>

#include "mako.h"
#include "icon.h"
#include "string-util.h"
#include "wayland.h"

#ifdef HAVE_ICONS

#include <keulim.h>

static bool validate_icon_name(const char* icon_name) {
	int icon_len = strlen(icon_name);
	if (icon_len > 1024) {
		return false;
	}
	int index;
	for (index = 0; index < icon_len; index ++) {
		bool is_number = icon_name[index] >= '0' && icon_name[index] <= '9';
		bool is_abc = (icon_name[index] >= 'A' && icon_name[index] <= 'Z') ||
				(icon_name[index] >= 'a' && icon_name[index] <= 'z');
		bool is_other = icon_name[index] == '-'
				|| icon_name[index] == '.' || icon_name[index] == '_';

		bool is_legal = is_number || is_abc || is_other;
		if (!is_legal) {
			return false;
		}
	}
	return true;
}

static uint8_t to_premult(uint8_t x, uint8_t a) {
	return (uint8_t)roundf((float)x * (float)a / 0xFF);
}

/**
 * Create a cairo surface from 8-bit unpacked RGB(A) pixel data, with straight
 * alpha.
 */
static cairo_surface_t *create_surface_from_data(const uint8_t *src_pixels,
		size_t width, size_t height, size_t src_stride, bool has_alpha) {
	size_t src_bytes_per_pixel = has_alpha ? 4 : 3;
	cairo_format_t format = has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

	cairo_surface_t *surface = cairo_image_surface_create(format, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Failed to create cairo surface\n");
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_surface_flush(surface);

	uint8_t *dst_pixels = cairo_image_surface_get_data(surface);
	int dst_stride = cairo_image_surface_get_stride(surface);
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			const uint8_t *src = &src_pixels[y * src_stride + x * src_bytes_per_pixel];
			uint8_t *dst = &dst_pixels[y * dst_stride + x * sizeof(uint32_t)];

			uint8_t r = src[0];
			uint8_t g = src[1];
			uint8_t b = src[2];
			uint8_t a = has_alpha ? src[3] : 0;

			// Convert from straight alpha to pre-multiplied alpha
			r = to_premult(r, a);
			g = to_premult(g, a);
			b = to_premult(b, a);

			// Convert from unpacked RGBA to native-endian packed ARGB
			uint32_t packed = 0;
			packed |= (uint32_t)r << 16;
			packed |= (uint32_t)g << 8;
			packed |= b;
			packed |= (uint32_t)a << 24;

			memcpy(dst, &packed, sizeof(packed));
		}
	}

	cairo_surface_mark_dirty(surface);

	return surface;
}

static cairo_surface_t *load_image(const char *path) {
	if (strlen(path) == 0) {
		return NULL;
	}

	FILE *f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "Failed to open icon\n");
		goto error;
	}

	struct klm_decoder *dec = klm_decoder_create_with_file(f);
	if (dec == NULL) {
		fprintf(stderr, "Failed to create icon decoder\n");
		fclose(f);
		goto error;
	}

	struct klm_info info;
	if (!klm_decoder_read_info(dec, &info)) {
		fprintf(stderr, "Failed to decode icon info\n");
		goto error_dec;
	}

	enum klm_format format;
	size_t bytes_per_pixel = 0;
	bool has_alpha = false;
	for (size_t i = 0; i < info.formats_len; i++) {
		format = info.formats[i];
		if (format == KLM_FORMAT_R8G8B8) {
			bytes_per_pixel = 3;
			break;
		} else if (format == KLM_FORMAT_R8G8B8A8) {
			bytes_per_pixel = 4;
			has_alpha = true;
			break;
		}
	}
	if (bytes_per_pixel == 0) {
		fprintf(stderr, "Unsupported icon pixel format\n");
		goto error_dec;
	}

	size_t stride = bytes_per_pixel * info.width;
	size_t size = stride * info.height;
	uint8_t *buffer = malloc(size);
	if (buffer == NULL) {
		perror("Failed to allocate buffer");
		goto error_buffer;
	}

	struct klm_decoder_read_frame_options options = {
		.format = format,
		.buffer = buffer,
		.size = size,
		.stride = stride,
	};
	if (!klm_decoder_read_frame(dec, &options)) {
		fprintf(stderr, "Failed to decode icon frame\n");
		goto error_buffer;
	}

	cairo_surface_t *surface = create_surface_from_data(buffer,
		info.width, info.height, stride, has_alpha);
	if (surface == NULL) {
		goto error_buffer;
	}

	free(buffer);
	klm_decoder_destroy(dec);
	return surface;

error_buffer:
	free(buffer);
error_dec:
	klm_decoder_destroy(dec);
error:
	fprintf(stderr, "Failed to load icon from %s\n", path);
	return NULL;
}

static cairo_surface_t *load_image_data(struct mako_image_data *image_data) {
	if (image_data->bits_per_sample != 8) {
		fprintf(stderr, "Unsupported number of bits per sample\n");
		return NULL;
	}
	if ((image_data->has_alpha && image_data->channels != 4) ||
			(!image_data->has_alpha && image_data->channels != 3)) {
		fprintf(stderr, "Unsupported number of channels\n");
		return NULL;
	}

	cairo_surface_t *surface = create_surface_from_data(image_data->data,
		image_data->width, image_data->height, image_data->rowstride, image_data->has_alpha);
	if (surface == NULL) {
		return NULL;
	}

	return surface;
}

static double fit_to_square(int width, int height, int square_size) {
	double longest = width > height ? width : height;
	return longest > square_size ? square_size/longest : 1.0;
}

static char hex_val(char digit) {
	assert(isxdigit(digit));
	if (digit >= 'a') {
		return digit - 'a' + 10;
	} else if (digit >= 'A') {
		return digit - 'A' + 10;
	} else {
		return digit - '0';
	}
}

static void url_decode(char *dst, const char *src) {
	while (src[0]) {
		if (src[0] == '%' && isxdigit(src[1]) && isxdigit(src[2])) {
			dst[0] = 16*hex_val(src[1]) + hex_val(src[2]);
			dst++; src += 3;
		} else {
			dst[0] = src[0];
			dst++; src++;
		}
	}
	dst[0] = '\0';
}

// Attempt to find a full path for a notification's icon_name, which may be:
// - An absolute path, which will simply be returned (as a new string)
// - A file:// URI, which will be converted to an absolute path
// - A Freedesktop icon name, which will be resolved within the configured
//   `icon-path` using something that looks vaguely like the algorithm defined
//   in the icon theme spec (https://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html)
//
// Returns the resolved path, or NULL if it was unable to find an icon. The
// return value must be freed by the caller.
static char *resolve_icon(struct mako_notification *notif) {
	char *icon_name = notif->app_icon;
	if (icon_name[0] == '\0') {
		return NULL;
	}
	if (icon_name[0] == '/') {
		return strdup(icon_name);
	}
	if (strstr(icon_name, "file://") == icon_name) {
		// Chop off the scheme and URL decode
		char *icon_path = malloc(strlen(icon_name) + 1 - strlen("file://"));
		if (icon_path == NULL) {
			return icon_path;
		}

		url_decode(icon_path, icon_name + strlen("file://"));
		return icon_path;
	}

	// Determine the largest scale factor of any attached output.
	int32_t max_scale = 1;
	struct mako_output *output = NULL;
	wl_list_for_each(output, &notif->state->outputs, link) {
		if (output->scale > max_scale) {
			max_scale = output->scale;
		}
	}

	static const char fallback[] = "%s:/usr/share/icons/hicolor";
	char *search = mako_asprintf(fallback, notif->style.icon_path);

	char *saveptr = NULL;
	char *theme_path = strtok_r(search, ":", &saveptr);

	// Match all icon files underneath of the theme_path followed by any icon
	// size and category subdirectories. This pattern assumes that all the
	// files in the icon path are valid icon types.
	static const char pattern_fmt[] = "%s/*/*/%s.*";

	char *icon_path = NULL;
	int32_t last_icon_size = 0;

	if (!validate_icon_name(icon_name)) {
		return NULL;
	}

	while (theme_path) {
		if (strlen(theme_path) == 0) {
			continue;
		}

		glob_t icon_glob = {0};
		char *pattern = mako_asprintf(pattern_fmt, theme_path, icon_name);

		// Disable sorting because we're going to do our own anyway.
		int found = glob(pattern, GLOB_NOSORT, NULL, &icon_glob);
		size_t found_count = 0;
		if (found == 0) {
			// The value of gl_pathc isn't guaranteed to be usable if glob
			// returns non-zero.
			found_count = icon_glob.gl_pathc;
		}

		for (size_t i = 0; i < found_count; ++i) {
			char *relative_path = icon_glob.gl_pathv[i];

			// Find the end of the current search path and walk to the next
			// path component. Hopefully this will be the icon resolution
			// subdirectory.
			relative_path += strlen(theme_path);
			while (relative_path[0] == '/') {
				++relative_path;
			}

			errno = 0;
			int32_t icon_size = strtol(relative_path, NULL, 10);
			if (errno || icon_size == 0) {
				// Try second level subdirectory if failed.
				errno = 0;
				while (relative_path[0] != '/') {
					++relative_path;
				}
				++relative_path;
				icon_size = strtol(relative_path, NULL, 10);
				if (errno || icon_size == 0) {
					continue;
				}
			}

			int32_t icon_scale = 1;
			char *scale_str = strchr(relative_path, '@');
			if (scale_str != NULL) {
				icon_scale = strtol(scale_str + 1, NULL, 10);
			}

			if (icon_size == notif->style.max_icon_size &&
					icon_scale == max_scale) {
				// If we find an exact match, we're done.
				free(icon_path);
				icon_path = strdup(icon_glob.gl_pathv[i]);
				break;
			} else if (icon_size < notif->style.max_icon_size * max_scale &&
					icon_size > last_icon_size) {
				// Otherwise, if this icon is small enough to fit but bigger
				// than the last best match, choose it on a provisional basis.
				// We multiply by max_scale to increase the odds of finding an
				// icon which looks sharp on the highest-scale output.
				free(icon_path);
				icon_path = strdup(icon_glob.gl_pathv[i]);
				last_icon_size = icon_size;
			}
		}

		free(pattern);
		globfree(&icon_glob);

		if (icon_path) {
			// The spec says that if we find any match whatsoever in a theme,
			// we should stop there to avoid mixing icons from different
			// themes even if one is a better size.
			break;
		}
		theme_path = strtok_r(NULL, ":", &saveptr);
	}

	if (icon_path == NULL) {
		// Finally, fall back to looking in /usr/share/pixmaps. These are
		// unsized icons, which may lead to downscaling, but some apps are
		// still using it.
		static const char pixmaps_fmt[] = "/usr/share/pixmaps/%s.*";

		char *pattern = mako_asprintf(pixmaps_fmt, icon_name);

		glob_t icon_glob = {0};
		int found = glob(pattern, GLOB_NOSORT, NULL, &icon_glob);

		if (found == 0 && icon_glob.gl_pathc > 0) {
			icon_path = strdup(icon_glob.gl_pathv[0]);
		}
		free(pattern);
		globfree(&icon_glob);
	}

	free(search);
	return icon_path;
}

struct mako_icon *create_icon(struct mako_notification *notif) {
	cairo_surface_t *surface = NULL;
	if (notif->image_data != NULL) {
		surface = load_image_data(notif->image_data);
	}

	if (surface == NULL) {
		char *path = resolve_icon(notif);
		if (path == NULL) {
			return NULL;
		}

		surface = load_image(path);
		free(path);
		if (surface == NULL) {
			return NULL;
		}
	}

	int image_width = cairo_image_surface_get_width(surface);
	int image_height = cairo_image_surface_get_height(surface);

	struct mako_icon *icon = calloc(1, sizeof(struct mako_icon));
	icon->scale = fit_to_square(image_width, image_height, notif->style.max_icon_size);
	icon->width = image_width * icon->scale;
	icon->height = image_height * icon->scale;
	icon->image = surface;
	return icon;
}
#else
struct mako_icon *create_icon(struct mako_notification *notif) {
	return NULL;
}
#endif

void draw_icon(cairo_t *cairo, struct mako_icon *icon,
		double xpos, double ypos, double scale) {
	cairo_save(cairo);
	cairo_scale(cairo, scale*icon->scale, scale*icon->scale);
	cairo_set_source_surface(cairo, icon->image, xpos/icon->scale, ypos/icon->scale);
	cairo_paint(cairo);
	cairo_restore(cairo);
}

void destroy_icon(struct mako_icon *icon) {
	if (icon != NULL) {
		if (icon->image != NULL) {
			cairo_surface_destroy(icon->image);
		}
		free(icon);
	}
}
