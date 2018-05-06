#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "mako.h"
#include "dbus.h"
#include "render.h"

static int handle_get_capabilities(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	fprintf(stderr, "get capabilities\n");

	// TODO: support capabilities
	char *capabilities[] = {};

	sd_bus_message *reply = NULL;
	int ret = sd_bus_message_new_method_return(msg, &reply);
	if (ret < 0) {
		return ret;
	}

	ret = sd_bus_message_append_array(reply, 's', capabilities,
		sizeof(capabilities)/sizeof(capabilities[0]));
	if (ret < 0) {
		return ret;
	}

	return sd_bus_send(NULL, reply, NULL);
}

static int handle_notify(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	struct mako_state *state = data;
	fprintf(stderr, "notify\n");

	int ret = 0;

	char *app_name, *app_icon, *summary, *body;
	uint32_t replaces_id;
	ret = sd_bus_message_read(msg, "susss", &app_name, &replaces_id, &app_icon,
		&summary, &body);
	if (ret < 0) {
		return ret;
	}

	// TODO: read the other parameters

	struct mako_notification *notif = create_notification(state);
	if (notif == NULL) {
		return -1;
	}
	notif->app_name = app_name;
	notif->app_icon = app_icon;
	notif->summary = summary;
	notif->body = body;
	render(state);

	return sd_bus_reply_method_return(msg, "u", notif->id);
}

static int handle_close_notification(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	fprintf(stderr, "close notification\n");
	// TODO
	return 0;
}

static int handle_get_server_information(sd_bus_message *msg, void *data,
		sd_bus_error *ret_error) {
	fprintf(stderr, "get server information\n");

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

	ret = sd_bus_add_object_vtable(state->bus, &state->slot,
		"/org/freedesktop/Notifications", "org.freedesktop.Notifications",
		notifications_vtable, state);
	if (ret < 0) {
		fprintf(stderr, "Failed to issue method call: %s\n", strerror(-ret));
		goto error;
	}

	ret = sd_bus_request_name(state->bus, "org.freedesktop.Notifications", 0);
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
