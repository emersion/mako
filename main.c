#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"

static const char usage[] =
	"Usage: mako [options...]\n"
	"\n"
	"  -h, --help                      Show help message and quit.\n"
	"      --font <font>               Font family and size.\n"
	"      --background-color <color>  Background color.\n"
	"      --text-color <color>        Text color.\n"
	"      --width <px>                Notification width.\n"
	"      --height <px>               Max notification height.\n"
	"      --margin <px>[,<px>...]     Margin values, comma separated.\n"
	"                                  Up to four values, with the same\n"
	"                                  meaning as in CSS.\n"
	"      --padding <px>              Padding.\n"
	"      --border-size <px>          Border size.\n"
	"      --border-color <color>      Border color.\n"
	"      --markup <0|1>              Enable/disable markup.\n"
	"      --format <format>           Format string.\n"
	"      --hidden-format <format>    Format string.\n"
	"      --max-visible <n>           Max number of visible notifications.\n"
	"      --default-timeout <timeout> Default timeout in milliseconds.\n"
	"      --output <name>             Show notifications on this output.\n"
	"      --anchor <corner>           Corner of output to put notifications.\n"
	"\n"
	"Colors can be specified with the format #RRGGBB or #RRGGBBAA.\n";

static bool init(struct mako_state *state) {
	if (!init_dbus(state)) {
		return false;
	}
	if (!init_wayland(state)) {
		finish_dbus(state);
		return false;
	}
	if (!init_event_loop(&state->event_loop, state->bus, state->display)) {
		finish_dbus(state);
		finish_wayland(state);
		return false;
	}
	wl_list_init(&state->notifications);
	return true;
}

static void finish(struct mako_state *state) {
	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		destroy_notification(notif);
	}
	finish_event_loop(&state->event_loop);
	finish_wayland(state);
	finish_dbus(state);
}

static struct mako_event_loop *event_loop = NULL;

int main(int argc, char *argv[]) {
	struct mako_state state = {0};

	state.argc = argc;
	state.argv = argv;

	// This is a bit wasteful, but easier than special-casing the reload.
	init_default_config(&state.config);
	int ret = reload_config(&state.config, argc, argv);

	if (ret < 0) {
		return EXIT_FAILURE;
	} else if (ret > 0) {
		printf("%s", usage);
		return EXIT_SUCCESS;
	}

	if (!init(&state)) {
		finish_config(&state.config);
		return EXIT_FAILURE;
	}

	event_loop = &state.event_loop;

	ret = run_event_loop(&state.event_loop);

	finish(&state);
	finish_config(&state.config);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
