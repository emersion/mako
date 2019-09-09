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

#include "icon.h"

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

	if (strstr(state->config.superstyle.format, "%b") != NULL) {
		ret = sd_bus_message_append(reply, "s", "body");
		if (ret < 0) {
			return ret;
		}
	}

	if (state->config.superstyle.markup) {
		ret = sd_bus_message_append(reply, "s", "body-markup");
		if (ret < 0) {
			return ret;
		}
	}

	if (state->config.superstyle.actions) {
		ret = sd_bus_message_append(reply, "s", "actions");
		if (ret < 0) {
			return ret;
		}
	}

	if (state->config.superstyle.icons) {
		ret = sd_bus_message_append(reply, "s", "icon-static");
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
	set_dirty(state);
}

static int handle_notify(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;
	int ret = 0;

	const char *app_name, *app_icon, *summary, *body;
	uint32_t replaces_id;
	ret = sd_bus_message_read(msg, "susss", &app_name, &replaces_id, &app_icon,
		&summary, &body);
	if (ret < 0) {
		return ret;
	}

	struct mako_notification *notif = NULL;
	if (replaces_id > 0) {
		notif = get_notification(state, replaces_id);
	}

	if (notif) {
		reset_notification(notif);
	} else {
		// Either we had no replaces_id, or the id given was invalid. Either
		// way, make a new notification.
		replaces_id = 0; // In case they got lucky and passed the next id.
		notif = create_notification(state);
	}

	if (notif == NULL) {
		return -1;
	}

	notif->app_name = strdup(app_name);
	notif->app_icon = strdup(app_icon);
	notif->summary = strdup(summary);
	notif->body = strdup(body);

	// These fields may not be filled, so make sure they're valid strings.
	notif->category = strdup("");
	notif->desktop_entry = strdup("");

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
			free(notif->category);
			notif->category = strdup(category);
		} else if (strcmp(hint, "desktop-entry") == 0) {
			const char *desktop_entry = NULL;
			ret = sd_bus_message_read(msg, "v", "s", &desktop_entry);
			if (ret < 0) {
				return ret;
			}
			free(notif->desktop_entry);
			notif->desktop_entry = strdup(desktop_entry);
		} else if (strcmp(hint, "value") == 0) {
			int32_t progress = 0;
			ret = sd_bus_message_read(msg, "v", "i", &progress);
			if (ret < 0) {
				return ret;
			}
			notif->progress = progress;
		} else if (strcmp(hint, "image-path") == 0 ||
				strcmp(hint, "image_path") == 0) {  // Deprecated.
			const char *image_path = NULL;
			ret = sd_bus_message_read(msg, "v", "s", &image_path);
			if (ret < 0) {
				return ret;
			}
			// image-path is higher priority than app_icon, so just overwrite
			// it. We're guaranteed to be doing this after reading the "real"
			// app_icon. It's also lower priority than image-data, and that
			// will win over app_icon if provided.
			free(notif->app_icon);
			notif->app_icon = strdup(image_path);
		} else if (strcmp(hint, "image-data") == 0 ||
				strcmp(hint, "image_data") == 0 ||  // Deprecated.
				strcmp(hint, "icon_data") == 0) {  // Even more deprecated.
			ret = sd_bus_message_enter_container(msg, 'v', "(iiibiiay)");
			if (ret < 0) {
				return ret;
			}

			ret = sd_bus_message_enter_container(msg, 'r', "iiibiiay");
			if (ret < 0) {
				return ret;
			}

			struct mako_image_data *image_data = calloc(1, sizeof(struct mako_image_data));
			if (image_data == NULL) {
				return -1;
			}

			ret = sd_bus_message_read(msg, "iiibii", &image_data->width,
					&image_data->height, &image_data->rowstride,
					&image_data->has_alpha, &image_data->bits_per_sample,
					&image_data->channels);
			if (ret < 0) {
				free(image_data);
				return ret;
			}

			// Calculate the expected useful data length without padding in last row
			// len = size before last row + size of last row
			//     = (height - 1) * rowstride + width * ceil(channels * bits_pre_sample / 8.0)
			size_t image_len = (image_data->height - 1) * image_data->rowstride +
				image_data->width * ((image_data->channels *
				image_data->bits_per_sample + 7) / 8);
			uint8_t *data = calloc(image_len, sizeof(uint8_t));
			if (data == NULL) {
				free(image_data);
				return -1;
			}

			ret = sd_bus_message_enter_container(msg, 'a', "y");
			if (ret < 0) {
				free(data);
				free(image_data);
				return ret;
			}

			// Ignore the extra padding bytes in the last row if exist
			for (size_t index = 0; index < image_len; index++) {
				uint8_t tmp;
				ret = sd_bus_message_read(msg, "y", &tmp);
				if (ret < 0){
					free(data);
					free(image_data);
					return ret;
				}
				data[index] = tmp;
			}

			image_data->data = data;
			if (notif->image_data != NULL) {
				free(notif->image_data->data);
				free(notif->image_data);
			}
			notif->image_data = image_data;

			ret = sd_bus_message_exit_container(msg);
			if (ret < 0) {
				return ret;
			}

			ret = sd_bus_message_exit_container(msg);
			if (ret < 0) {
				return ret;
			}

			ret = sd_bus_message_exit_container(msg);
			if (ret < 0) {
				return ret;
			}
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

	// We can insert a notification prior to matching criteria, because sort is
	// global. We also know that inserting a notification into the global list
	// regardless of the configured sort criteria places it in the correct
	// position relative to any of its potential group mates even before
	// knowing what criteria we will be grouping them by (proof left as an
	// exercise to the reader).
	if (replaces_id != notif->id) {
		// Only insert notifcations if they're actually new, to avoid creating
		// duplicates in the list.
		insert_notification(state, notif);
	}

	int match_count = apply_each_criteria(&state->config.criteria, notif);
	if (match_count == -1) {
		// We encountered an allocation failure or similar while applying
		// criteria. The notification may be partially matched, but the worst
		// case is that it has an empty style, so bail.
		fprintf(stderr, "Failed to apply criteria\n");
		destroy_notification(notif);
		return -1;
	} else if (match_count == 0) {
		// This should be impossible, since the global criteria is always
		// present in a mako_config and matches everything.
		fprintf(stderr, "Notification matched zero criteria?!\n");
		destroy_notification(notif);
		return -1;
	}

	int32_t expire_timeout = notif->requested_timeout;
	if (expire_timeout < 0 || notif->style.ignore_timeout) {
		expire_timeout = notif->style.default_timeout;
	}

	if (expire_timeout > 0) {
		notif->timer = add_event_loop_timer(&state->event_loop, expire_timeout,
			handle_notification_timer, notif);
	}

	if (notif->style.icons) {
		notif->icon = create_icon(notif);
	}

	// Now we need to perform the grouping based on the new notification's
	// group criteria specification (list of critera which must match). We
	// don't necessarily want to start with the new notification, as depending
	// on the sort criteria, there may be matching ones earlier in the list.
	// After this call, the matching notifications will be contiguous in the
	// list, and the first one that matches will always still be first.
	struct mako_criteria *notif_criteria = create_criteria_from_notification(
			notif, &notif->style.group_criteria_spec);
	if (!notif_criteria) {
		destroy_notification(notif);
		return -1;
	}
	group_notifications(state, notif_criteria);
	free(notif_criteria);

	set_dirty(state);

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
		set_dirty(state);
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
	if (!action->notification->style.actions) {
		// Actions are disabled for this notification, bail.
		return;
	}

	struct mako_state *state = action->notification->state;

	sd_bus_emit_signal(state->bus, service_path, service_interface,
		"ActionInvoked", "us", action->notification->id, action->key);
}
