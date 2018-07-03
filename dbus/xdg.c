#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "criteria.h"
#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "wayland.h"

static const char *service_path = "/org/freedesktop/Notifications";
static const char *service_interface = "org.freedesktop.Notifications";

static int handle_get_capabilities(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	sd_bus_message *reply = NULL;
	int ret = sd_bus_message_new_method_return(msg, &reply);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_open_container(reply, 'a', "s");
	if (ret < 0) {
		return ret;
	}

	if (strstr(global_criteria(&state->config)->style.format, "%b") != NULL) {
		ret = sd_bus_message_append(reply, "s", "body");
		if (ret < 0) {
			return ret;
		}
	}

	if (global_criteria(&state->config)->style.markup) {
		ret = sd_bus_message_append(reply, "s", "body-markup");
		if (ret < 0) {
			return ret;
		}
	}

	if (global_criteria(&state->config)->style.actions) {
		ret = sd_bus_message_append(reply, "s", "actions");
		if (ret < 0) {
			return ret;
		}
	}

	ret = sd_bus_message_close_container(reply);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_send(NULL, reply, NULL);
	if (ret < 0) {
		return ret;
	}

	sd_bus_message_unref(reply);
	return 0;
}

static void handle_notification_timer(void *data) {
	struct mako_notification *notif = data;
	notif->timer = NULL;

	struct mako_state *state = notif->state;

	close_notification(notif, MAKO_NOTIFICATION_CLOSE_EXPIRED);
	send_frame(state);
}

static int handle_notify(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	struct mako_notification *notif = create_notification(state);
	if (notif == NULL) {
		return -1;
	}

	int ret = 0;

	const char *app_name, *app_icon, *summary, *body;
	uint32_t replaces_id;
	ret = sd_bus_message_read(msg, "susss", &app_name, &replaces_id, &app_icon,
		&summary, &body);
	if (ret < 0) {
		return ret;
	}
	notif->app_name = strdup(app_name);
	notif->app_icon = strdup(app_icon);
	notif->summary = strdup(summary);
	notif->body = strdup(body);

	if (replaces_id > 0) {
		struct mako_notification *replaces =
			get_notification(state, replaces_id);
		if (replaces) {
			close_notification(replaces, MAKO_NOTIFICATION_CLOSE_REQUEST);
		}
	}

	ret = sd_bus_message_enter_container(msg, 'a', "s");
	if (ret < 0) {
		return ret;
	}

	while (1) {
		const char *action_key, *action_title;
		ret = sd_bus_message_read(msg, "ss", &action_key, &action_title);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		struct mako_action *action = calloc(1, sizeof(struct mako_action));
		if (action == NULL) {
			return -1;
		}
		action->notification = notif;
		action->key = strdup(action_key);
		action->title = strdup(action_title);
		wl_list_insert(&notif->actions, &action->link);
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_enter_container(msg, 'a', "{sv}");
	if (ret < 0) {
		return ret;
	}

	while (1) {
		ret = sd_bus_message_enter_container(msg, 'e', "sv");
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		const char *hint = NULL;
		ret = sd_bus_message_read(msg, "s", &hint);
		if (ret < 0) {
			return ret;
		}

		if (strcmp(hint, "urgency") == 0) {
			// Should be a byte but some clients (Chromium) send an uint32_t
			const char *contents = NULL;
			ret = sd_bus_message_peek_type(msg, NULL, &contents);
			if (ret < 0) {
				return ret;
			}

			if (strcmp(contents, "u") == 0) {
				uint32_t urgency = 0;
				ret = sd_bus_message_read(msg, "v", "u", &urgency);
				if (ret < 0) {
					return ret;
				}
				notif->urgency = urgency;
			} else {
				uint8_t urgency = 0;
				ret = sd_bus_message_read(msg, "v", "y", &urgency);
				if (ret < 0) {
					return ret;
				}
				notif->urgency = urgency;
			}
		} else if (strcmp(hint, "category") == 0) {
			const char *category = NULL;
			ret = sd_bus_message_read(msg, "v", "s", &category);
			if (ret < 0) {
				return ret;
			}
			notif->category = strdup(category);
		} else if (strcmp(hint, "desktop-entry") == 0) {
			const char *desktop_entry = NULL;
			ret = sd_bus_message_read(msg, "v", "s", &desktop_entry);
			if (ret < 0) {
				return ret;
			}
			notif->desktop_entry = strdup(desktop_entry);
		} else {
			ret = sd_bus_message_skip(msg, "v");
			if (ret < 0) {
				return ret;
			}
		}

		ret = sd_bus_message_exit_container(msg);
		if (ret < 0) {
			return ret;
		}
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		return ret;
	}

	int32_t requested_timeout;
	ret = sd_bus_message_read(msg, "i", &requested_timeout);
	if (ret < 0) {
		return ret;
	}
	notif->requested_timeout = requested_timeout;

	int match_count = apply_each_criteria(&state->config.criteria, notif);
	if (match_count == -1) {
		// We encountered an allocation failure or similar while applying
		// criteria. The notification may be partially matched, but the worst
		// case is that it has an empty style, so bail.
		fprintf(stderr, "Failed to apply criteria\n");
		return -1;
	} else if (match_count == 0) {
		// This should be impossible, since the global criteria is always
		// present in a mako_config and matches everything.
		fprintf(stderr, "Notification matched zero criteria?!\n");
		return -1;
	}

	int32_t expire_timeout = notif->requested_timeout;
	if (expire_timeout < 0 || notif->style.ignore_timeout) {
		expire_timeout = notif->style.default_timeout;
	}

	insert_notification(state, notif);
	if (expire_timeout > 0) {
		notif->timer = add_event_loop_timer(&state->event_loop, expire_timeout,
			handle_notification_timer, notif);
	}

	send_frame(state);

	return sd_bus_reply_method_return(msg, "u", notif->id);
}

static int handle_close_notification(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	uint32_t id;
	int ret = sd_bus_message_read(msg, "u", &id);
	if (ret < 0) {
		return ret;
	}

	// TODO: check client
	struct mako_notification *notif = get_notification(state, id);
	if (notif) {
		close_notification(notif, MAKO_NOTIFICATION_CLOSE_REQUEST);
		send_frame(state);
	}

	return sd_bus_reply_method_return(msg, "");
}

static int handle_get_server_information(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	const char *name = "mako";
	const char *vendor = "emersion";
	const char *version = "0.0.0";
	const char *spec_version = "1.2";
	return sd_bus_reply_method_return(msg, "ssss", name, vendor, version,
		spec_version);
}

static const sd_bus_vtable service_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GetCapabilities", "", "as", handle_get_capabilities, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", handle_notify, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CloseNotification", "u", "", handle_close_notification, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetServerInformation", "", "ssss", handle_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("ActionInvoked", "us", 0),
	SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
	SD_BUS_VTABLE_END
};

int init_dbus_xdg(struct mako_state *state) {
	return sd_bus_add_object_vtable(state->bus, &state->xdg_slot, service_path,
		service_interface, service_vtable, state);
}

void notify_notification_closed(struct mako_notification *notif,
		enum mako_notification_close_reason reason) {
	struct mako_state *state = notif->state;

	sd_bus_emit_signal(state->bus, service_path, service_interface,
		"NotificationClosed", "uu", notif->id, reason);
}

void notify_action_invoked(struct mako_action *action) {
	struct mako_state *state = action->notification->state;

	sd_bus_emit_signal(state->bus, service_path, service_interface,
		"ActionInvoked", "us", action->notification->id, action->key);
}
