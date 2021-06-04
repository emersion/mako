#define _POSIX_C_SOURCE 200809L
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "cairo-pixbuf.h"

static GdkPixbuf *load_image(const char *path) {
	if (strlen(path) == 0) {
		return NULL;
	}
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		fprintf(stderr, "Failed to load icon (%s)\n", err->message);
		g_error_free(err);
		return NULL;
	}
	return pixbuf;
}

static GdkPixbuf *load_image_data(struct mako_image_data *image_data) {
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(image_data->data, GDK_COLORSPACE_RGB,
			image_data->has_alpha, image_data->bits_per_sample, image_data->width,
			image_data->height, image_data->rowstride, NULL, NULL);
	if (!pixbuf) {
		fprintf(stderr, "Failed to load icon\n");
		return NULL;
	}
	return pixbuf;
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
	GdkPixbuf *image = NULL;
	if (notif->image_data != NULL) {
		image = load_image_data(notif->image_data);
	}

	if (image == NULL) {
		char *path = resolve_icon(notif);
		if (path == NULL) {
			return NULL;
		}

		image = load_image(path);
		free(path);
		if (image == NULL) {
			return NULL;
		}
	}

	int image_width = gdk_pixbuf_get_width(image);
	int image_height = gdk_pixbuf_get_height(image);

	struct mako_icon *icon = calloc(1, sizeof(struct mako_icon));
	icon->scale = fit_to_square(
			image_width, image_height, notif->style.max_icon_size);
	icon->width = image_width * icon->scale;
	icon->height = image_height * icon->scale;

	icon->image = create_cairo_surface_from_gdk_pixbuf(image);
	g_object_unref(image);
	if (icon->image == NULL) {
		free(icon);
		return NULL;
	}

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
