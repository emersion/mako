#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "pango.h"
#include "render.h"

static void set_cairo_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
		(color >> (3*8) & 0xFF) / 255.0,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0);
}

void render(struct mako_state *state) {
	struct mako_config *config = state->config;

	if (wl_list_empty(&state->notifications)) {
		// Unmap the surface
		wl_surface_attach(state->surface, NULL, 0, 0);
		wl_surface_commit(state->surface);
		return;
	}

	state->current_buffer = get_next_buffer(state->shm, state->buffers,
		state->width, state->height);

	cairo_t *cairo = state->current_buffer->cairo;
	set_cairo_source_u32(cairo, config->colors.background);
	cairo_paint(cairo);

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		set_cairo_source_u32(cairo, config->colors.text);
		cairo_move_to(cairo, config->padding, config->padding);
		printf_pango(cairo, config->font, 1, true, "%s", notif->summary);
		break; // TODO: support multiple notifications
	}

	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}
