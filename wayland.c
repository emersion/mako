#include <stdio.h>

#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"

static void noop() {
	// This space intentionally left blank
}


static void pointer_handle_button(void *data, struct wl_pointer *pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t button_state) {
	struct mako_state *state = data;

	if (button_state != WL_POINTER_BUTTON_STATE_PRESSED ||
			wl_list_empty(&state->notifications)) {
		return;
	}

	struct mako_notification *first_notif =
		wl_container_of(state->notifications.next, first_notif, link);
	close_notification(first_notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
	send_frame(state);
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = noop,
	.leave = noop,
	.motion = noop,
	.button = pointer_handle_button,
	.axis = noop,
};


static void seat_handle_capabilities(void *data, struct wl_seat *seat,
		uint32_t capabilities) {
	struct mako_state *state = data;

	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		struct wl_pointer *pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(pointer, &pointer_listener, state);
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
};


static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct mako_state *state = data;

	state->width = width;
	state->height = height;

	zwlr_layer_surface_v1_ack_configure(surface, serial);
	send_frame(state);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct mako_state *state = data;
	state->running = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};


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
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct wl_seat *seat =
			wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(seat, &seat_listener, state);
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

	state->registry = wl_display_get_registry(state->display);
	wl_registry_add_listener(state->registry, &registry_listener, state);
	wl_display_roundtrip(state->display);

	if (state->compositor == NULL) {
		fprintf(stderr, "compositor doesn't support wl_compositor\n");
		return false;
	}
	if (state->shm == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm\n");
		return false;
	}
	if (state->layer_shell == NULL) {
		fprintf(stderr, "compositor doesn't support zwlr_layer_shell_v1\n");
		return false;
	}

	state->surface = wl_compositor_create_surface(state->compositor);
	state->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		state->layer_shell, state->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		"notifications");

	zwlr_layer_surface_v1_add_listener(state->layer_surface,
		&layer_surface_listener, state);

	struct mako_config *config = &state->config;
	int32_t margin = config->margin;
	zwlr_layer_surface_v1_set_size(state->layer_surface,
		config->width, config->height);
	zwlr_layer_surface_v1_set_anchor(state->layer_surface,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_margin(state->layer_surface,
		margin, margin, margin, margin);
	wl_surface_commit(state->surface);

	return true;
}

void finish_wayland(struct mako_state *state) {
	zwlr_layer_surface_v1_destroy(state->layer_surface);
	wl_surface_destroy(state->surface);
	destroy_buffer(&state->buffers[0]);
	destroy_buffer(&state->buffers[1]);

	wl_shm_destroy(state->shm);
	zwlr_layer_shell_v1_destroy(state->layer_shell);
	wl_compositor_destroy(state->compositor);
	wl_registry_destroy(state->registry);
	wl_display_disconnect(state->display);
}

void send_frame(struct mako_state *state) {
	state->current_buffer = get_next_buffer(state->shm, state->buffers,
		state->width, state->height);

	int height = render(state, state->current_buffer);

	if (height == 0) {
		// Unmap the surface
		wl_surface_attach(state->surface, NULL, 0, 0);
		wl_surface_commit(state->surface);
		return;
	}

	// TODO: if the compositor doesn't send a configure with the size we
	// requested, we'll enter an infinite loop
	if (state->height != height) {
		zwlr_layer_surface_v1_set_size(state->layer_surface,
			state->config.width, height);
		wl_surface_commit(state->surface);
		return;
	}

	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}
