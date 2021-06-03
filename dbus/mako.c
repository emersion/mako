#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "criteria.h"
#include "surface.h"
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
	struct mako_surface *surface;

	wl_list_for_each(surface, &state->surfaces, link) {
		set_dirty(surface);
	}

	return sd_bus_reply_method_return(msg, "");
}

static int handle_dismiss_group_notifications(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	if (wl_list_empty(&state->notifications)) {
		goto done;
	}

	struct mako_notification *notif =
		wl_container_of(state->notifications.next, notif, link);

	struct mako_surface *surface = notif->surface;
	close_group_notifications(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
	set_dirty(surface);

done:
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
	set_dirty(notif->surface);

done:
	return sd_bus_reply_method_return(msg, "");
}

static int handle_dismiss_notification(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	uint32_t id = 0;
	int dismiss_group = 0;
	int ret = sd_bus_message_read(msg, "ub", &id, &dismiss_group);
	if (ret < 0) {
		return ret;
	}

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->id == id || id == 0) {
			if (dismiss_group) {
				close_group_notifications(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
			} else {
				close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
			}
			set_dirty(notif->surface);
			break;
		}
	}

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

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->id == id || id == 0) {
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

	return sd_bus_reply_method_return(msg, "");
}

static int handle_restore_action(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;

	if (wl_list_empty(&state->history)) {
		goto done;
	}

	struct mako_notification *notif =
		wl_container_of(state->history.next, notif, link);
	wl_list_remove(&notif->link);

	insert_notification(state, notif);
	set_dirty(notif->surface);

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

		ret = sd_bus_message_append(reply, "{sv}", "category",
			"s", notif->category);
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

/**
 * The way surfaces are re-build here is not quite intuitive.
 * 1. All surfaces are destroyed.
 * 2. The styles and surface association of notifications is recomputed.
 *    This will also (re)create all surfaces we need in the new config.
 * 3. Start the redraw events.
 */
static void reapply_config(struct mako_state *state) {
	struct mako_surface *surface, *tmp;
	wl_list_for_each_safe(surface, tmp, &state->surfaces, link) {
		destroy_surface(surface);
	}

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		// Reset the notifications' grouped state so that if criteria have been
		// removed they'll separate properly.
		notif->group_index = -1;
		/* Also reset the notif->surface so it gets reasigned to default
		 * if appropriate */
		notif->surface = NULL;

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

	wl_list_for_each(surface, &state->surfaces, link) {
		set_dirty(surface);
	}
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

static const sd_bus_vtable service_vtable[] = {
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("DismissAllNotifications", "", "", handle_dismiss_all_notifications, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DismissGroupNotifications", "", "", handle_dismiss_group_notifications, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DismissLastNotification", "", "", handle_dismiss_last_notification, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("DismissNotification", "ub", "", handle_dismiss_notification, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("InvokeAction", "us", "", handle_invoke_action, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("RestoreNotification", "", "", handle_restore_action, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("ListNotifications", "", "aa{sv}", handle_list_notifications, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("Reload", "", "", handle_reload, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END
};

int init_dbus_mako(struct mako_state *state) {
	return sd_bus_add_object_vtable(state->bus, &state->mako_slot, service_path,
		service_interface, service_vtable, state);
}
