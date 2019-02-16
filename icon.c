#include <stdio.h>
#include <assert.h>
#include <cairo/cairo.h>

#include "icon.h"

#ifdef SHOW_ICONS
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

GdkPixbuf *load_image(const char *path) {
	if (strlen(path) == 0) {
		return NULL;
	}
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		fprintf(stderr, "Failed to load icon (%s)\n", err->message);
		return NULL;
	}
	return pixbuf;
}

struct mako_icon get_icon(const char *path, double max_size) {
	struct mako_icon icon;
	icon.image = load_image(path);
	if (icon.image == NULL)
		return icon;

	icon.image_width = gdk_pixbuf_get_width(icon.image);
	icon.image_height = gdk_pixbuf_get_height(icon.image);

	if (icon.image_width > icon.image_height) {
		// Width limits size
		if (icon.image_width > max_size) {
			icon.width = max_size;
			icon.scale = icon.width/icon.image_width;
			icon.height = icon.image_height*icon.scale;
		} else {
			icon.width = icon.image_width;
			icon.height = icon.image_height;
		}
	} else {
		// Height limits size
		if (icon.image_height > max_size) {
			icon.height = max_size;
			icon.scale = icon.height/icon.image_height;
			icon.width = icon.image_width*icon.scale;
		} else {
			icon.width = icon.image_width;
			icon.height = icon.image_height;
		}
	}

	return icon;
}

void draw_icon(cairo_t *cairo, struct mako_icon icon,
		double xpos, double ypos, double scale) {
	cairo_save(cairo);
	cairo_scale(cairo, icon.scale * scale, icon.scale * scale);
	gdk_cairo_set_source_pixbuf(cairo, icon.image, xpos / icon.scale, ypos / icon.scale);
	cairo_paint(cairo);
	cairo_restore(cairo);
}

void destroy_icon(struct mako_icon icon) {
	if (icon.image !=  NULL) {
		g_object_unref(icon.image);
	}
}

#else

struct mako_icon get_icon(const char *path, double max_size) {
	struct mako_icon icon;
	icon.image = NULL;
	return icon;
}

void draw_icon(cairo_t *cairo, struct mako_icon icon,
		double xpos, double ypos, double scale) {}

void destroy_icon(struct mako_icon icon) {}
#endif

