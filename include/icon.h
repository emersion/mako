#ifndef MAKO_ICON_H
#define MAKO_ICON_H

#include <cairo/cairo.h>

struct mako_icon {
	double width;
	double height;
	double image_width;
	double image_height;
	double scale;
	cairo_surface_t* image;
};

struct mako_icon get_icon(const char *path, double max_size);

#endif
