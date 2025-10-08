#ifndef MAKO_NOTIFICATION_H
#define MAKO_NOTIFICATION_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "config.h"
#include "types.h"

struct mako_state;
struct mako_surface;
struct mako_timer;
struct mako_criteria;
struct mako_icon;

struct mako_hotspot {
	int32_t x, y;
	int32_t width, height;
};

struct mako_notification {
	struct mako_state *state;
	struct mako_surface *surface;
	struct wl_list link; // mako_state::notifications

	struct mako_style style;
	struct mako_icon *icon;

	uint32_t id;
	int group_index;
	int group_count;
	int group_focus_index;
	bool hidden;

	char *app_name;
	char *app_icon;
	char *summary;
	char *body;
	int32_t requested_timeout;
	struct wl_list actions; // mako_action::link

	enum mako_notification_urgency urgency;
	char *category;
	char *desktop_entry;
	char *tag;
	int32_t progress;
	struct mako_image_data *image_data;

	struct mako_hotspot hotspot;
	struct mako_timer *timer;
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

// Tiny struct to be the data type for format_hidden_text.
struct mako_hidden_format_data {
	size_t hidden;
	size_t count;
};

struct mako_binding_context {
	struct mako_surface *surface;
	struct mako_seat *seat;
	uint32_t serial;
};

typedef char *(*mako_format_func_t)(char variable, bool *markup, void *data);

bool hotspot_at(struct mako_hotspot *hotspot, int32_t x, int32_t y);

void reset_notification(struct mako_notification *notif);
struct mako_notification *create_notification(struct mako_state *state);
void destroy_notification(struct mako_notification *notif);

void close_notification(struct mako_notification *notif,
	enum mako_notification_close_reason reason,
	bool add_to_history);
void close_group_notifications(struct mako_notification *notif,
	enum mako_notification_close_reason reason, bool add_to_history);
void close_all_notifications(struct mako_state *state,
	enum mako_notification_close_reason reason, bool add_to_history);
char *format_hidden_text(char variable, bool *markup, void *data);
char *format_notif_text(char variable, bool *markup, void *data);
size_t format_text(const char *format, char *buf, mako_format_func_t func, void *data);
struct mako_notification *get_notification(struct mako_state *state, uint32_t id);
struct mako_notification *get_tagged_notification(struct mako_state *state, const char *tag, const char *app_name);
size_t format_notification(struct mako_notification *notif, const char *format,
	char *buf);
void notification_handle_button(struct mako_notification *notif, uint32_t button,
	enum wl_pointer_button_state state, const struct mako_binding_context *ctx);
void notification_handle_axis(struct mako_notification *notif, uint32_t axis,
	wl_fixed_t value, const struct mako_binding_context *ctx);
void notification_handle_touch(struct mako_notification *notif,
	const struct mako_binding_context *ctx);
void notification_execute_binding(struct mako_notification *notif,
	const struct mako_binding *binding, const struct mako_binding_context *ctx);
void insert_notification(struct mako_state *state, struct mako_notification *notif);
int group_notifications(struct mako_state *state, struct mako_criteria *criteria);

#endif
