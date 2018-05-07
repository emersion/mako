#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "render.h"

static const char *service_path = "/org/freedesktop/Notifications";
static const char *service_interface = "org.freedesktop.Notifications";
static const char *service_name = "org.freedesktop.Notifications";

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

	if (strstr(state->config.format, "%b") != NULL) {
		ret = sd_bus_message_append(reply, "s", "body");
		if (ret < 0) {
			return ret;
		}
	}

	if (state->config.markup) {
		ret = sd_bus_message_append(reply, "s", "body-markup");
		if (ret < 0) {
			return ret;
		}
	}

	ret = sd_bus_message_close_container(reply);
	if (ret < 0) {
		return ret;
	}

	return sd_bus_send(NULL, reply, NULL);
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
		const char *action_id, *action_title;
		ret = sd_bus_message_read(msg, "ss", &action_id, &action_title);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}

		struct mako_action *action = calloc(1, sizeof(struct mako_action));
		if (action == NULL) {
			return -1;
		}
		action->id = strdup(action_id);
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

		const char *hint;
		ret = sd_bus_message_read(msg, "s", &hint);
		if (ret < 0) {
			return ret;
		}

		if (strcmp(hint, "urgency") == 0) {
			uint8_t urgency = 0;
			ret = sd_bus_message_read(msg, "v", "y", &urgency);
			if (ret < 0) {
				return ret;
			}
			notif->urgency = urgency;
		} else if (strcmp(hint, "category") == 0) {
			const char *category;
			ret = sd_bus_message_read(msg, "v", "s", &category);
			if (ret < 0) {
				return ret;
			}
			notif->category = strdup(category);
		} else if (strcmp(hint, "desktop-entry") == 0) {
			const char *desktop_entry;
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

	int32_t expire_timeout;
	ret = sd_bus_message_read(msg, "i", &expire_timeout);
	if (ret < 0) {
		return ret;
	}
	// TODO: timeout

	render(state);

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
		render(state);
	}
	return 0;
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

static const sd_bus_vtable notifications_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("GetCapabilities", "", "as", handle_get_capabilities, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", handle_notify, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("CloseNotification", "u", "", handle_close_notification, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetServerInformation", "", "ssss", handle_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_SIGNAL("ActionInvoked", "us", 0),
	SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
	SD_BUS_VTABLE_END
};

bool init_dbus(struct mako_state *state) {
	int ret = 0;
	state->bus = NULL;
	state->slot = NULL;

	ret = sd_bus_open_user(&state->bus);
	if (ret < 0) {
		fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-ret));
		goto error;
	}

	ret = sd_bus_add_object_vtable(state->bus, &state->slot, service_path,
		service_interface, notifications_vtable, state);
	if (ret < 0) {
		fprintf(stderr, "Failed to issue method call: %s\n", strerror(-ret));
		goto error;
	}

	ret = sd_bus_request_name(state->bus, service_name, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-ret));
		goto error;
	}

	return true;

error:
	finish_dbus(state);
	return false;
}

void finish_dbus(struct mako_state *state) {
	sd_bus_slot_unref(state->slot);
	sd_bus_unref(state->bus);
}

void notify_notification_closed(struct mako_notification *notif,
		enum mako_notification_close_reason reason) {
	struct mako_state *state = notif->state;

	sd_bus_emit_signal(state->bus, service_path, service_interface,
		"NotificationClosed", "uu", notif->id, reason);
}
