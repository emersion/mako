#include <errno.h>
#include <stdio.h>

#include "dbus.h"
#include "mako.h"

static const char service_name[] = "org.freedesktop.Notifications";

bool init_dbus(struct mako_state *state) {
	int ret = 0;
	state->bus = NULL;
	state->xdg_slot = state->mako_slot = NULL;

	ret = sd_bus_open_user(&state->bus);
	if (ret < 0) {
		fprintf(stderr, "Failed to connect to user bus: %s\n", strerror(-ret));
		goto error;
	}

	ret = init_dbus_xdg(state);
	if (ret < 0) {
		fprintf(stderr, "Failed to initialize XDG interface: %s\n", strerror(-ret));
		goto error;
	}

	ret = init_dbus_mako(state);
	if (ret < 0) {
		fprintf(stderr, "Failed to initialize Mako interface: %s\n", strerror(-ret));
		goto error;
	}

	ret = sd_bus_request_name(state->bus, service_name, 0);
	if (ret < 0) {
		if (ret == -EXIST) {
			fprintf(stderr, "Failed to acquire service name: %s. Is Mako already running?\n", strerror(-ret));
		} else {
			fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-ret));
		}
		goto error;
	}

	return true;

error:
	finish_dbus(state);
	return false;
}

void finish_dbus(struct mako_state *state) {
	sd_bus_slot_unref(state->xdg_slot);
	sd_bus_slot_unref(state->mako_slot);
	sd_bus_flush_close_unref(state->bus);
}
