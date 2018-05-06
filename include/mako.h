#ifndef _MAKO_H
#define _MAKO_H

#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client.h>

#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct mako_config;

struct mako_state {
	bool running;
	struct mako_config *config;

	sd_bus *bus;
	sd_bus_slot *slot;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	int32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	uint32_t last_id;
	struct wl_list notifications; // mako_notification::link
};

struct mako_config {
	char *font;
	int32_t margin;

	struct {
		uint32_t background;
		uint32_t text;
	} colors;
};

enum mako_notification_urgency {
	MAKO_NOTIFICATION_URGENCY_LOW = 0,
	MAKO_NOTIFICATION_URGENCY_NORMAL = 1,
	MAKO_NOTIFICATION_URGENCY_HIGH = 2,
	MAKO_NOTIFICATION_URGENCY_UNKNWON = -1,
};

struct mako_notification {
	struct mako_state *state;
	struct wl_list link; // mako_state::notifications

	uint32_t id;
	char *app_name;
	char *app_icon;
	char *summary;
	char *body;
	struct wl_list actions; // mako_action::link

	enum mako_notification_urgency urgency;
};

struct mako_action {
	struct wl_list link; // mako_notification::actions
	char *id;
	char *title;
};

enum mako_notification_close_reason {
	MAKO_NOTIFICATION_CLOSE_EXPIRED = 1,
	MAKO_NOTIFICATION_CLOSE_DISMISSED = 2,
	MAKO_NOTIFICATION_CLOSE_REQUEST = 3,
	MAKO_NOTIFICATION_CLOSE_UNKNOWN = 4,
};

struct mako_notification *create_notification(struct mako_state *state);
void insert_notification(struct mako_notification *notif);
void destroy_notification(struct mako_notification *notif);
void close_notification(struct mako_notification *notif,
	enum mako_notification_close_reason reason);
struct mako_notification *get_notification(struct mako_state *state, uint32_t id);

#endif
