#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "render.h"

void render_notification(struct mako_notification *notif) {
	notif->current_buffer = get_next_buffer(notif->state->shm, notif->buffers,
		notif->width, notif->height);

	cairo_t *cairo = notif->current_buffer->cairo;
	cairo_set_source_rgb(cairo, 1.0, 0.0, 0.0);
	cairo_paint(cairo);

	wl_surface_attach(notif->surface, notif->current_buffer->buffer, 0, 0);
	wl_surface_damage(notif->surface, 0, 0, notif->width, notif->height);
	wl_surface_commit(notif->surface);
}
