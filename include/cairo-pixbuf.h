#ifndef MAKO_CAIRO_PIXBUF_H
#define MAKO_CAIRO_PIXBUF_H

#ifndef HAVE_ICONS
#error "gdk_pixbuf is required"
#endif

#include <cairo/cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

cairo_surface_t *create_cairo_surface_from_gdk_pixbuf(const GdkPixbuf *pixbuf);

#endif
