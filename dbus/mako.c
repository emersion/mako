#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "criteria.h"
#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "wayland.h"

static const char *service_path = "/fr/emersion/Mako";
static const char *service_interface = "fr.emersion.Mako";

static int handle_dismiss_all_notifications(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	close_all_notifications(state, MAKO_NOTIFICATION_CLOSE_DISMISSED);
	set_dirty(state);

	return sd_bus_reply_method_return(msg, "");
}

static int handle_dismiss_last_notification(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	if (wl_list_empty(&state->notifications)) {
		goto done;
	}

	struct mako_notification *notif =
		wl_container_of(state->notifications.next, notif, link);
	close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
	set_dirty(state);

done:
	return sd_bus_reply_method_return(msg, "");
}

static int handle_invoke_action(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	uint32_t id = 0;
	const char *action_key;
	int ret = sd_bus_message_read(msg, "us", &id, &action_key);
	if (ret < 0) {
		return ret;
	}

	if (id == 0) {
		id = state->last_id;
	}

	if (wl_list_empty(&state->notifications)) {
		goto done;
	}

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->id == id) {
			struct mako_action *action;
			wl_list_for_each(action, &notif->actions, link) {
				if (strcmp(action->key, action_key) == 0) {
					notify_action_invoked(action);
					break;
				}
			}
			break;
		}
	}

done:
	return sd_bus_reply_method_return(msg, "");
}

static int handle_list_notifications(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	sd_bus_message *reply = NULL;
	int ret = sd_bus_message_new_method_return(msg, &reply);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_open_container(reply, 'a', "a{sv}");
	if (ret < 0) {
		return ret;
	}

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		ret = sd_bus_message_open_container(reply, 'a', "{sv}");
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append(reply, "{sv}", "app-name",
			"s", notif->app_name);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append(reply, "{sv}", "app-icon",
			"s", notif->app_icon);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append(reply, "{sv}", "summary",
			"s", notif->summary);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append(reply, "{sv}", "body",
			"s", notif->body);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append(reply, "{sv}", "id",
			"u", notif->id);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_open_container(reply, 'e', "sv");
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_append_basic(reply, 's', "actions");
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_open_container(reply, 'v', "a{ss}");
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_open_container(reply, 'a', "{ss}");
		if (ret < 0) {
			return ret;
		}

		struct mako_action *action;
		wl_list_for_each(action, &notif->actions, link) {
			ret = sd_bus_message_append(reply, "{ss}", action->key, action->title);
			if (ret < 0) {
				return ret;
			}
		}

		ret = sd_bus_message_close_container(reply);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_close_container(reply);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_close_container(reply);
		if (ret < 0) {
			return ret;
		}

		ret = sd_bus_message_close_container(reply);
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

static void reapply_config(struct mako_state *state) {
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		// Reset the notifications' grouped state so that if criteria have been
		// removed they'll separate properly.
		notif->group_index = -1;

		finish_style(&notif->style);
		init_empty_style(&notif->style);
		apply_each_criteria(&state->config.criteria, notif);

		// Having to do this for every single notification really hurts... but
		// it does do The Right Thing (tm).
		struct mako_criteria *notif_criteria = create_criteria_from_notification(
				notif, &notif->style.group_criteria_spec);
		if (!notif_criteria) {
			continue;
		}
		group_notifications(state, notif_criteria);
		free(notif_criteria);
	}

	set_dirty(state);
}

static int handle_reload(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	if (reload_config(&state->config, state->argc, state->argv) != 0) {
		sd_bus_error_set_const(
				ret_error, "fr.emersion.Mako.InvalidConfig",
				"Unable to parse configuration file");
		return -1;
	}

	reapply_config(state);

	return sd_bus_reply_method_return(msg, "");
}

static int handle_set_config_option(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	const char *name = NULL, *value = NULL;
	int ret = sd_bus_message_read(msg, "ss", &name, &value);
	if (ret < 0) {
		return ret;
	}

	if (!apply_global_option(&state->config, name, value)) {
		sd_bus_error_set_const(ret_error, "fr.emersion.Mako.InvalidConfig",
			"Failed to apply configuration option");
		return -1;
	}

	reapply_config(state);

	return sd_bus_reply_method_return(msg, "");
}

static const sd_bus_vtable service_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("DismissAllNotifications", "", "", handle_dismiss_all_notifications, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DismissLastNotification", "", "", handle_dismiss_last_notification, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("InvokeAction", "us", "", handle_invoke_action, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ListNotifications", "", "aa{sv}", handle_list_notifications, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Reload", "", "", handle_reload, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetConfigOption", "ss", "", handle_set_config_option, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int init_dbus_mako(struct mako_state *state) {
	return sd_bus_add_object_vtable(state->bus, &state->mako_slot, service_path,
		service_interface, service_vtable, state);
}
