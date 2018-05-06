#ifndef _MAKO_H
#define _MAKO_H

#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client.h>

#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct mako_state {
	sd_bus *bus;
	sd_bus_slot *slot;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;

	uint32_t last_id;
	struct wl_list notifications; // mako_notification::link
};

struct mako_notification {
	struct mako_state *state;
	struct wl_list link; // mako_state::notifications

	uint32_t id;
	char *app_name;
	char *app_icon;
	char *summary;
	char *body;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	int32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
};

struct mako_notification *create_notification(struct mako_state *state);
void insert_notification(struct mako_notification *notif);
void destroy_notification(struct mako_notification *notif);

#endif
