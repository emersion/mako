#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "pango.h"
#include "render.h"

void render(struct mako_state *state) {
	if (wl_list_empty(&state->notifications)) {
		// Unmap the surface
		wl_surface_attach(state->surface, NULL, 0, 0);
		wl_surface_commit(state->surface);
		return;
	}

	state->current_buffer = get_next_buffer(state->shm, state->buffers,
		state->width, state->height);

	cairo_t *cairo = state->current_buffer->cairo;
	cairo_set_source_rgb(cairo, 0.0, 0.0, 0.0);
	cairo_paint(cairo);

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		printf_pango(cairo, "", 1, true, "%s", notif->summary);
		break; // TODO
	}

	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}
