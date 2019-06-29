#ifndef MAKO_H
#define MAKO_H

#include <stdbool.h>
#include <wayland-client.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#endif

#include "config.h"
#include "event-loop.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "set.h"

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
	struct wl_list outputs; // mako_output::link
	struct wl_list seats; // mako_seat::link

	struct wl_surface *surface;
	struct mako_output *surface_output;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct mako_output *layer_surface_output;
	bool configured;
	bool frame_pending; // Have we requested a frame callback?
	bool dirty; // Do we need to redraw?
	int32_t scale;

	int32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	uint32_t last_id;
	struct mako_set_top *replaced_ids_set;
	struct wl_list notifications; // mako_notification::link

	int argc;
	char **argv;
};

#endif
