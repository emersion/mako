#ifndef MAKO_ICON_H
#define MAKO_ICON_H

#include <cairo/cairo.h>

#ifdef SHOW_ICONS
#include <gdk-pixbuf/gdk-pixbuf.h>
#else
typedef void GdkPixbuf;
#endif

struct mako_icon {
	double width;
	double height;
	double image_width;
	double image_height;
	double scale;
	GdkPixbuf* image;
};

struct mako_icon get_icon(const char *path, double max_size);
void destroy_icon(struct mako_icon icon);
void destroy_icon(struct mako_icon icon);
void draw_icon(cairo_t *cairo, struct mako_icon icon,
		double xpos, double ypos, double scale);

#endif
