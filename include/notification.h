#ifndef _MAKO_NOTIFICATION_H
#define _MAKO_NOTIFICATION_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

enum mako_notification_urgency {
	MAKO_NOTIFICATION_URGENCY_LOW = 0,
	MAKO_NOTIFICATION_URGENCY_NORMAL = 1,
	MAKO_NOTIFICATION_URGENCY_HIGH = 2,
	MAKO_NOTIFICATION_URGENCY_UNKNWON = -1,
};

struct mako_state;

struct mako_hotspot {
	int32_t x, y;
	int32_t width, height;
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
	char *category;
	char *desktop_entry;

	struct mako_hotspot hotspot;
};

struct mako_action {
	struct mako_notification *notification;
	struct wl_list link; // mako_notification::actions
	char *key;
	char *title;
};

enum mako_notification_close_reason {
	MAKO_NOTIFICATION_CLOSE_EXPIRED = 1,
	MAKO_NOTIFICATION_CLOSE_DISMISSED = 2,
	MAKO_NOTIFICATION_CLOSE_REQUEST = 3,
	MAKO_NOTIFICATION_CLOSE_UNKNOWN = 4,
};

#define DEFAULT_ACTION_KEY "default"

bool hotspot_at(struct mako_hotspot *hotspot, int32_t x, int32_t y);

struct mako_notification *create_notification(struct mako_state *state);
void destroy_notification(struct mako_notification *notif);
void close_notification(struct mako_notification *notif,
	enum mako_notification_close_reason reason);
void close_all_notifications(struct mako_state *state,
	enum mako_notification_close_reason reason);
struct mako_notification *get_notification(struct mako_state *state, uint32_t id);
size_t format_notification(struct mako_notification *notif, const char *format,
	char *buf);
void notification_handle_button(struct mako_notification *notif, uint32_t button,
	enum wl_pointer_button_state state);

#endif
