#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "criteria.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "surface.h"
#include "wayland.h"

static void noop() {
	// This space intentionally left blank
}


static void xdg_output_handle_name(void *data, struct zxdg_output_v1 *xdg_output,
		const char *name) {
	struct mako_output *output = data;
	output->name = strdup(name);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = noop,
	.logical_size = noop,
	.done = noop,
	.name = xdg_output_handle_name,
	.description = noop,
};

static void get_xdg_output(struct mako_output *output) {
	if (output->state->xdg_output_manager == NULL ||
			output->xdg_output != NULL) {
		return;
	}

	output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
		output->state->xdg_output_manager, output->wl_output);
	zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener,
		output);
}

static void output_handle_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t phy_width, int32_t phy_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct mako_output *output = data;
	output->subpixel = subpixel;
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
		int32_t factor) {
	struct mako_output *output = data;
	output->scale = factor;
}

static const struct wl_output_listener output_listener = {
	.geometry = output_handle_geometry,
	.mode = noop,
	.done = noop,
	.scale = output_handle_scale,
};

static void create_output(struct mako_state *state,
		struct wl_output *wl_output, uint32_t global_name) {
	struct mako_output *output = calloc(1, sizeof(struct mako_output));
	if (output == NULL) {
		fprintf(stderr, "allocation failed\n");
		return;
	}
	output->state = state;
	output->global_name = global_name;
	output->wl_output = wl_output;
	output->scale = 1;
	wl_list_insert(&state->outputs, &output->link);

	wl_output_set_user_data(wl_output, output);
	wl_output_add_listener(wl_output, &output_listener, output);
	get_xdg_output(output);
}

static void destroy_output(struct mako_output *output) {
	struct mako_surface *surface;
	wl_list_for_each(surface, &output->state->surfaces, link) {
		if (surface->surface_output == output) {
			surface->surface_output = NULL;
		}
		if (surface->layer_surface_output == output) {
			surface->layer_surface_output = NULL;
		}
	}
	wl_list_remove(&output->link);
	if (output->xdg_output != NULL) {
		zxdg_output_v1_destroy(output->xdg_output);
	}
	wl_output_destroy(output->wl_output);
	free(output->name);
	free(output);
}

static struct mako_surface *get_surface(struct mako_state *state,
		struct wl_surface *wl_surface) {
	struct mako_surface *surface;
	wl_list_for_each(surface, &state->surfaces, link) {
		if (surface->surface == wl_surface) {
			return surface;
		}
	}
	return NULL;
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct mako_seat *seat = data;
	if (id >= MAX_TOUCHPOINTS) {
		return;
	}
	seat->touch.pts[id].x = wl_fixed_to_int(surface_x);
	seat->touch.pts[id].y = wl_fixed_to_int(surface_y);
}

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, struct wl_surface *wl_surface,
		int32_t id, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct mako_seat *seat = data;
	if (id >= MAX_TOUCHPOINTS) {
		return;
	}
	seat->touch.pts[id].x = wl_fixed_to_int(surface_x);
	seat->touch.pts[id].y = wl_fixed_to_int(surface_y);
	seat->touch.pts[id].surface = get_surface(seat->state, wl_surface);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id) {
	struct mako_seat *seat = data;
	struct mako_state *state = seat->state;

	if (id >= MAX_TOUCHPOINTS) {
		return;
	}

	const struct mako_binding_context ctx = {
		.surface = seat->touch.pts[id].surface,
		.seat = seat,
		.serial = serial,
	};

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (hotspot_at(&notif->hotspot, seat->touch.pts[id].x, seat->touch.pts[id].y)) {
			struct mako_surface *surface = notif->surface;
			notification_handle_touch(notif, &ctx);
			set_dirty(surface);
			break;
		}
	}

	seat->touch.pts[id].surface = NULL;
}

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *wl_surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct mako_seat *seat = data;
	struct mako_state *state = seat->state;

	seat->pointer.x = wl_fixed_to_int(surface_x);
	seat->pointer.y = wl_fixed_to_int(surface_y);
	seat->pointer.surface = get_surface(state, wl_surface);

	// Change the mouse cursor to "left_ptr"
	if (state->cursor_theme != NULL) {
		wl_pointer_set_cursor(wl_pointer, serial, state->cursor_surface,
			state->cursor_image->hotspot_x, state->cursor_image->hotspot_y);
	}
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *wl_surface) {
	struct mako_seat *seat = data;
	seat->pointer.surface = NULL;
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct mako_seat *seat = data;
	seat->pointer.x = wl_fixed_to_int(surface_x);
	seat->pointer.y = wl_fixed_to_int(surface_y);
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button,
		uint32_t button_state) {
	struct mako_seat *seat = data;
	struct mako_state *state = seat->state;

	const struct mako_binding_context ctx = {
		.surface = seat->pointer.surface,
		.seat = seat,
		.serial = serial,
	};

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (hotspot_at(&notif->hotspot, seat->pointer.x, seat->pointer.y)) {
			struct mako_surface *surface = notif->surface;
			notification_handle_button(notif, button, button_state, &ctx);
			set_dirty(surface);
			break;
		}
	}
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = noop,
};

static const struct wl_touch_listener touch_listener = {
	.down = touch_handle_down,
	.up = touch_handle_up,
	.motion = touch_handle_motion,
	.frame = noop,
	.cancel = noop,
	.shape = noop,
	.orientation = noop,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		uint32_t capabilities) {
	struct mako_seat *seat = data;

	if (seat->pointer.wl_pointer != NULL) {
		wl_pointer_release(seat->pointer.wl_pointer);
		seat->pointer.wl_pointer = NULL;
	}
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		seat->pointer.wl_pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(seat->pointer.wl_pointer,
			&pointer_listener, seat);
	}
	if (seat->touch.wl_touch != NULL) {
		wl_touch_release(seat->touch.wl_touch);
		seat->touch.wl_touch = NULL;
	}
	if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
		seat->touch.wl_touch = wl_seat_get_touch(wl_seat);
		wl_touch_add_listener(seat->touch.wl_touch,
			&touch_listener, seat);
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = noop,
};

static void create_seat(struct mako_state *state, struct wl_seat *wl_seat) {
	struct mako_seat *seat = calloc(1, sizeof(struct mako_seat));
	if (seat == NULL) {
		fprintf(stderr, "allocation failed\n");
		return;
	}
	seat->state = state;
	seat->wl_seat = wl_seat;
	wl_list_insert(&state->seats, &seat->link);
	wl_seat_add_listener(wl_seat, &seat_listener, seat);
}

static void destroy_seat(struct mako_seat *seat) {
	wl_list_remove(&seat->link);
	wl_seat_release(seat->wl_seat);
	if (seat->pointer.wl_pointer) {
		wl_pointer_release(seat->pointer.wl_pointer);
	}
	free(seat);
}

static void surface_handle_enter(void *data, struct wl_surface *surface,
		struct wl_output *wl_output) {
	struct mako_surface *msurface = data;
	// Don't bother keeping a list of outputs, a layer surface can only be on
	// one output a a time
	msurface->surface_output = wl_output_get_user_data(wl_output);
	set_dirty(msurface);
}

static void surface_handle_leave(void *data, struct wl_surface *surface,
		struct wl_output *wl_output) {
	struct mako_surface *msurface = data;
	msurface->surface_output = NULL;
}

static const struct wl_surface_listener surface_listener = {
	.enter = surface_handle_enter,
	.leave = surface_handle_leave,
};


static void schedule_frame_and_commit(struct mako_surface *state);
static void send_frame(struct mako_surface *surface);

static void layer_surface_handle_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct mako_surface *msurface = data;

	msurface->configured = true;
	msurface->width = width;
	msurface->height = height;

	zwlr_layer_surface_v1_ack_configure(surface, serial);
	send_frame(msurface);
}

static void layer_surface_handle_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct mako_surface *msurface = data;

	zwlr_layer_surface_v1_destroy(msurface->layer_surface);
	msurface->layer_surface = NULL;

	wl_surface_destroy(msurface->surface);
	msurface->surface = NULL;

	if (msurface->frame_callback) {
		wl_callback_destroy(msurface->frame_callback);
		msurface->frame_callback = NULL;
		msurface->dirty = true;
	}

	if (msurface->configured) {
		msurface->configured = false;
		msurface->width = msurface->height = 0;
		msurface->dirty = true;
	}

	if (msurface->dirty) {
		schedule_frame_and_commit(msurface);
	}
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_handle_configure,
	.closed = layer_surface_handle_closed,
};


static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct mako_state *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name,
			&wl_shm_interface, 1);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(registry, name,
			&zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct wl_seat *seat =
			wl_registry_bind(registry, name, &wl_seat_interface, 3);
		create_seat(state, seat);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *output =
			wl_registry_bind(registry, name, &wl_output_interface, 3);
		create_output(state, output, name);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
			version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
		state->xdg_output_manager = wl_registry_bind(registry, name,
			&zxdg_output_manager_v1_interface,
			ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
	} else if (strcmp(interface, xdg_activation_v1_interface.name) == 0) {
		state->xdg_activation = wl_registry_bind(registry, name,
			&xdg_activation_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	struct mako_state *state = data;

	struct mako_output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &state->outputs, link) {
		if (output->global_name == name) {
			destroy_output(output);
			break;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

bool init_wayland(struct mako_state *state) {
	wl_list_init(&state->outputs);
	wl_list_init(&state->seats);

	state->display = wl_display_connect(NULL);

	if (state->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return false;
	}

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

	if (state->xdg_output_manager != NULL) {
		struct mako_output *output;
		wl_list_for_each(output, &state->outputs, link) {
			get_xdg_output(output);
		}
		wl_display_roundtrip(state->display);
	}
	if (state->xdg_output_manager == NULL) {
		struct mako_criteria *criteria;
		wl_list_for_each(criteria, &state->config.criteria, link) {
			if (criteria->style.spec.output &&
					strcmp(criteria->style.output, "") != 0) {
				fprintf(stderr, "warning: configured an output "
					"but compositor doesn't support "
					"xdg-output-unstable-v1 version 2\n");
				break;
			}
		}
	}

	// Set up the cursor. It needs a wl_surface with the cursor loaded into it.
	// If one of these fail, mako will work fine without the cursor being able to change.
	const char *cursor_size_env = getenv("XCURSOR_SIZE");
	int cursor_size = 24;
	if (cursor_size_env != NULL) {
		errno = 0;
		char *end;
		int temp_size = (int)strtol(cursor_size_env, &end, 10);
		if (errno == 0 && cursor_size_env[0] != 0 && end[0] == 0 && temp_size > 0) {
			cursor_size = temp_size;
		} else {
			fprintf(stderr, "Error: XCURSOR_SIZE is invalid\n");
		}
	}
	state->cursor_theme = wl_cursor_theme_load(NULL, cursor_size, state->shm);
	if (state->cursor_theme == NULL) {
		fprintf(stderr, "couldn't find a cursor theme\n");
		return true;
	}
	struct wl_cursor *cursor = wl_cursor_theme_get_cursor(state->cursor_theme, "left_ptr");
	if (cursor == NULL) {
		fprintf(stderr, "couldn't find cursor icon \"left_ptr\"\n");
		wl_cursor_theme_destroy(state->cursor_theme);
		// Set to NULL so it doesn't get free'd again
		state->cursor_theme = NULL;
		return true;
	}
	state->cursor_image = cursor->images[0];
	struct wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(cursor->images[0]);
	state->cursor_surface = wl_compositor_create_surface(state->compositor);
	wl_surface_attach(state->cursor_surface, cursor_buffer, 0, 0);
	wl_surface_commit(state->cursor_surface);

	return true;
}

void finish_wayland(struct mako_state *state) {
	struct mako_surface *surface, *stmp;
	wl_list_for_each_safe(surface, stmp, &state->surfaces, link) {
		destroy_surface(surface);
	}

	struct mako_output *output, *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state->outputs, link) {
		destroy_output(output);
	}

	struct mako_seat *seat, *seat_tmp;
	wl_list_for_each_safe(seat, seat_tmp, &state->seats, link) {
		destroy_seat(seat);
	}

	if (state->xdg_activation != NULL) {
		xdg_activation_v1_destroy(state->xdg_activation);
	}
	if (state->xdg_output_manager != NULL) {
		zxdg_output_manager_v1_destroy(state->xdg_output_manager);
	}

	if (state->cursor_theme != NULL) {
		wl_cursor_theme_destroy(state->cursor_theme);
		wl_surface_destroy(state->cursor_surface);
	}

	zwlr_layer_shell_v1_destroy(state->layer_shell);
	wl_compositor_destroy(state->compositor);
	wl_shm_destroy(state->shm);
	wl_registry_destroy(state->registry);
	wl_display_disconnect(state->display);
}

static struct wl_region *get_input_region(struct mako_surface *surface) {
	struct wl_region *region =
		wl_compositor_create_region(surface->state->compositor);

	struct mako_notification *notif;
	wl_list_for_each(notif, &surface->state->notifications, link) {
		struct mako_hotspot *hotspot = &notif->hotspot;
		if (notif->surface == surface) {
			wl_region_add(region, hotspot->x, hotspot->y,
				hotspot->width, hotspot->height);
		}
	}

	return region;
}

static struct mako_output *get_configured_output(struct mako_surface *surface) {
	const char *output_name = surface->configured_output;
	if (strcmp(output_name, "") == 0) {
		return NULL;
	}

	struct mako_output *output;
	wl_list_for_each(output, &surface->state->outputs, link) {
		if (output->name != NULL && strcmp(output->name, output_name) == 0) {
			return output;
		}
	}

	return NULL;
}

static void schedule_frame_and_commit(struct mako_surface *surface);

// Draw and commit a new frame.
static void send_frame(struct mako_surface *surface) {
	struct mako_state *state = surface->state;

	int scale = 1;
	if (surface->surface_output != NULL) {
		scale = surface->surface_output->scale;
	}

	surface->current_buffer =
		get_next_buffer(state->shm, surface->buffers,
		surface->width * scale, surface->height * scale);
	if (surface->current_buffer == NULL) {
		fprintf(stderr, "no buffer available\n");
		return;
	}

	struct mako_output *output = get_configured_output(surface);
	int height = render(surface, surface->current_buffer, scale);

	// There are two cases where we want to tear down the surface: zero
	// notifications (height = 0) or moving between outputs.
	if (height == 0 || surface->layer_surface_output != output) {
		if (surface->layer_surface != NULL) {
			zwlr_layer_surface_v1_destroy(surface->layer_surface);
			surface->layer_surface = NULL;
		}
		if (surface->surface != NULL) {
			wl_surface_destroy(surface->surface);
			surface->surface = NULL;
		}
		surface->width = surface->height = 0;
		surface->surface_output = NULL;
		surface->configured = false;
	}

	// If there are no notifications, there's no point in recreating the
	// surface right now.
	if (height == 0) {
		surface->dirty = false;
		return;
	}

	// If we've made it here, there is something to draw. If the surface
	// doesn't exist (this is the first notification, or we moved to a
	// different output), we need to create it.
	if (surface->layer_surface == NULL) {
		struct wl_output *wl_output = NULL;
		if (output != NULL) {
			wl_output = output->wl_output;
		}
		surface->layer_surface_output = output;

		surface->surface = wl_compositor_create_surface(state->compositor);
		wl_surface_add_listener(surface->surface, &surface_listener, surface);

		surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			state->layer_shell, surface->surface, wl_output,
			surface->layer, "notifications");
		zwlr_layer_surface_v1_add_listener(surface->layer_surface,
			&layer_surface_listener, surface);

		// Because we're creating a new surface, we aren't going to draw
		// anything into it during this call. We don't know what size the
		// surface will be until we've asked the compositor for what we want
		// and it has responded with what it actually gave us. We also know
		// that the height we would _like_ to draw (greater than zero, or we
		// would have bailed already) is different from our state->height
		// (which has to be zero here), so we can fall through to the next
		// block to let it set the size for us.
	}

	assert(surface->layer_surface);

	// We now want to resize the surface if it isn't the right size. If the
	// surface is brand new, it doesn't even have a size yet. If it already
	// exists, we might need to resize if the list of notifications has changed
	// since the last time we drew.
	if (surface->height != height) {
		struct mako_style *style = &state->config.superstyle;

		zwlr_layer_surface_v1_set_size(surface->layer_surface,
				style->width + style->margin.left + style->margin.right,
				height);
		zwlr_layer_surface_v1_set_anchor(surface->layer_surface,
				surface->anchor);
		wl_surface_commit(surface->surface);

		// Now we're going to bail without drawing anything. This gives the
		// compositor a chance to create the surface and tell us what size we
		// were actually granted, which may be smaller than what we asked for
		// depending on the screen size and layout of other layer surfaces.
		// This information is provided in layer_surface_handle_configure,
		// which will then call send_frame again. When that call happens, the
		// layer surface will exist and the height will hopefully match what
		// we asked for. That means we won't return here, and will actually
		// draw into the surface down below.
		// TODO: If the compositor doesn't send a configure with the size we
		// requested, we'll enter an infinite loop. We need to keep track of
		// the fact that a request was sent separately from what height we are.
		return;
	}

	assert(surface->configured);

	// Yay we can finally draw something!
	struct wl_region *input_region = get_input_region(surface);
	wl_surface_set_input_region(surface->surface, input_region);
	wl_region_destroy(input_region);

	wl_surface_set_buffer_scale(surface->surface, scale);
	wl_surface_damage_buffer(surface->surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	surface->current_buffer->busy = true;

	// Schedule a frame in case the state becomes dirty again
	schedule_frame_and_commit(surface);

	surface->dirty = false;
}

static void frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	struct mako_surface *surface = data;

	wl_callback_destroy(surface->frame_callback);
	surface->frame_callback = NULL;

	// Only draw again if we need to
	if (surface->dirty) {
		send_frame(surface);
	}
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_handle_done,
};

static void schedule_frame_and_commit(struct mako_surface *surface) {
	if (surface->frame_callback) {
		return;
	}
	if (surface->surface == NULL) {
		// We don't yet have a surface, create it immediately
		send_frame(surface);
		return;
	}
	surface->frame_callback = wl_surface_frame(surface->surface);
	wl_callback_add_listener(surface->frame_callback, &frame_listener, surface);
	wl_surface_commit(surface->surface);
}

void set_dirty(struct mako_surface *surface) {
	if (surface->dirty) {
		return;
	}
	surface->dirty = true;
	schedule_frame_and_commit(surface);
}

static void activation_token_handle_done(void *data,
		struct xdg_activation_token_v1 *token, const char *token_str) {
	char **out = data;
	*out = strdup(token_str);
}

static const struct xdg_activation_token_v1_listener activation_token_listener = {
	.done = activation_token_handle_done,
};

char *create_xdg_activation_token(struct mako_surface *surface,
		struct mako_seat *seat, uint32_t serial) {
	struct mako_state *state = seat->state;
	if (state->xdg_activation == NULL) {
		return NULL;
	}

	char *token_str = NULL;
	struct xdg_activation_token_v1 *token =
		xdg_activation_v1_get_activation_token(state->xdg_activation);
	xdg_activation_token_v1_add_listener(token, &activation_token_listener,
		&token_str);
	xdg_activation_token_v1_set_serial(token, serial, seat->wl_seat);
	xdg_activation_token_v1_set_surface(token, surface->surface);
	xdg_activation_token_v1_commit(token);

	while (wl_display_dispatch(state->display) >= 0 && token_str == NULL) {
		// This space is intentionally left blank
	}

	xdg_activation_token_v1_destroy(token);

	return token_str;
}
