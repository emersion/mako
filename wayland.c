#include <stdio.h>

#include "mako.h"
#include "render.h"

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct mako_state *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 3);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name,
			&wl_shm_interface, 1);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(registry, name,
			&zwlr_layer_shell_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

bool init_wayland(struct mako_state *state) {
	state->display = wl_display_connect(NULL);

	struct wl_registry *registry = wl_display_get_registry(state->display);
	wl_registry_add_listener(registry, &registry_listener, state);
	wl_display_roundtrip(state->display);

	return true;
}

void finish_wayland(struct mako_state *state) {
	wl_shm_destroy(state->shm);
	zwlr_layer_shell_v1_destroy(state->layer_shell);
	wl_compositor_destroy(state->compositor);
	wl_registry_destroy(state->registry);
	wl_display_disconnect(state->display);
}


static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct mako_notification *notif = data;
	fprintf(stderr, "configure\n");
	notif->width = width;
	notif->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_notification(notif);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct mako_notification *notif = data;
	fprintf(stderr, "closed\n");
	destroy_notification(notif);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

bool init_wayland_notification(struct mako_notification *notif) {
	struct mako_state *state = notif->state;

	notif->surface = wl_compositor_create_surface(state->compositor);
	notif->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		state->layer_shell, notif->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		"notifications");

	zwlr_layer_surface_v1_add_listener(notif->layer_surface,
		&layer_surface_listener, notif);

	// TODO: size
	zwlr_layer_surface_v1_set_size(notif->layer_surface, 300, 100);
	zwlr_layer_surface_v1_set_anchor(notif->layer_surface,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_margin(notif->layer_surface, 10, 10, 10, 10);
	wl_surface_commit(notif->surface);

	return true;
}

void finish_wayland_notification(struct mako_notification *notif) {
	wl_surface_destroy(notif->surface);
	zwlr_layer_surface_v1_destroy(notif->layer_surface);
}
