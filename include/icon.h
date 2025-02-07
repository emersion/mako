#ifndef MAKO_ICON_H
#define MAKO_ICON_H

#include <cairo/cairo.h>
#include "notification.h"

struct mako_icon {
	double width;
	double height;
	double scale;
	int32_t border_radius;
	cairo_surface_t *image;
};

struct mako_image_data {
	int32_t width;
	int32_t height;
	int32_t rowstride;
	uint32_t has_alpha;
	int32_t bits_per_sample;
	int32_t channels;
	uint8_t *data;
};

struct mako_icon *create_icon(struct mako_notification *notif);
void destroy_icon(struct mako_icon *icon);
void draw_icon(cairo_t *cairo, struct mako_icon *icon,
		double xpos, double ypos, double scale);

#endif
