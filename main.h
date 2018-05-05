#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client.h>

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
};

struct mako_state mako;

bool init_dbus(struct mako_state *state);
void finish_dbus(struct mako_state *state);

bool init_wayland(struct mako_state *state);
void finish_wayland(struct mako_state *state);
