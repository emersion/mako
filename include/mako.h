#ifndef _MAKO_H
#define _MAKO_H

#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client.h>

#include "config.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct mako_state {
	bool running;
	struct mako_config config;

	sd_bus *bus;
	sd_bus_slot *slot;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	int32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	uint32_t last_id;
	struct wl_list notifications; // mako_notification::link
};

#endif
