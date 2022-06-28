#ifndef MAKO_H
#define MAKO_H

#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#if defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#elif defined(HAVE_LIBELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_BASU)
#include <basu/sd-bus.h>
#endif

#include "config.h"
#include "event-loop.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"

struct mako_state;

struct mako_surface {
	struct wl_list link;

	struct mako_state *state;

	struct wl_surface *surface;
	struct mako_output *surface_output;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct mako_output *layer_surface_output;
	struct wl_callback *frame_callback;
	bool configured;
	bool dirty; // Do we need to redraw?
	int32_t scale;

	char *configured_output;
	enum zwlr_layer_shell_v1_layer layer;
	uint32_t anchor;

	int32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
};

struct mako_state {
	struct mako_config config;
	struct mako_event_loop event_loop;

	sd_bus *bus;
	sd_bus_slot *xdg_slot, *mako_slot;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zxdg_output_manager_v1 *xdg_output_manager;
	struct xdg_activation_v1 *xdg_activation;
	struct wl_list outputs; // mako_output::link
	struct wl_list seats; // mako_seat::link

	struct {
		uint32_t size;
		uint32_t scale;
		struct wl_cursor_theme *theme;
		const struct wl_cursor_image *image;
		struct wl_surface *surface;
	} cursor;

	struct wl_list surfaces; // mako_surface::link

	uint32_t last_id;
	struct wl_list notifications; // mako_notification::link
	struct wl_list history; // mako_notification::link
	char *current_mode;

	int argc;
	char **argv;
};

#endif
