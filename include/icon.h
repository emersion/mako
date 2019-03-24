#ifndef MAKO_ICON_H
#define MAKO_ICON_H

#include <cairo/cairo.h>
#include "notification.h"

struct mako_icon {
	double width;
	double height;
	double scale;
	cairo_surface_t *image;
};

struct mako_icon *create_icon(struct mako_notification *notif);
void destroy_icon(struct mako_icon *icon);
void draw_icon(cairo_t *cairo, struct mako_icon *icon,
		double xpos, double ypos, double scale);

#endif
