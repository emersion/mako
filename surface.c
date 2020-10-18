#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "mako.h"
#include "surface.h"

void destroy_surface(struct mako_surface *surface) {
	if (surface->layer_surface != NULL) {
		zwlr_layer_surface_v1_destroy(surface->layer_surface);
	}
	if (surface->surface != NULL) {
		wl_surface_destroy(surface->surface);
	}
	finish_buffer(&surface->buffers[0]);
	finish_buffer(&surface->buffers[1]);

	/* Clean up memory resources */
	free(surface->configured_output);
	wl_list_remove(&surface->link);
	free(surface);
}

struct mako_surface *create_surface(struct mako_state *state, const char *output,
		enum zwlr_layer_shell_v1_layer layer, uint32_t anchor) {
	struct mako_surface *surface = calloc(1, sizeof(*surface));
	if (!surface) {
		return NULL;
	}

	surface->configured_output = strdup(output);
	surface->layer = layer;
	surface->anchor = anchor;
	surface->state = state;

	wl_list_insert(&state->surfaces, &surface->link);
	return surface;
}
