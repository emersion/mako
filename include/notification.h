#ifndef _MAKO_NOTIFICATION_H
#define _MAKO_NOTIFICATION_H

#include <wayland-client.h>

enum mako_notification_urgency {
	MAKO_NOTIFICATION_URGENCY_LOW = 0,
	MAKO_NOTIFICATION_URGENCY_NORMAL = 1,
	MAKO_NOTIFICATION_URGENCY_HIGH = 2,
	MAKO_NOTIFICATION_URGENCY_UNKNWON = -1,
};

struct mako_state;

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
