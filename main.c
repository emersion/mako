#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"

static const char usage[] =
	"Usage: mako [options...]\n"
	"\n"
	"  -h, --help                     Show help message and quit.\n"
	"      --font <font>              Font family and size.\n"
	"      --background-color <color> Background color.\n"
	"      --text-color <color>       Text color.\n"
	"      --width <px>               Notification width.\n"
	"      --height <px>              Max notification height.\n"
	"      --margin <px>[,<px>...]    Margin values, comma separated.\n"
	"                                 Up to four values, with the same\n"
	"                                 meaning as in CSS.\n"
	"      --padding <px>             Padding.\n"
	"      --border-size <px>         Border size.\n"
	"      --border-color <color>     Border color.\n"
	"      --markup <0|1>             Enable/disable markup.\n"
	"      --format <format>          Format string.\n"
	"      --max-visible <n>          Max number of visible notifications.\n"
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
	init_event_loop(&state->event_loop, state->bus, state->display);
	wl_list_init(&state->notifications);
	return true;
}

static void finish(struct mako_state *state) {
	finish_event_loop(&state->event_loop);
	finish_wayland(state);
	finish_dbus(state);
	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		destroy_notification(notif);
	}
}

static struct mako_event_loop *event_loop = NULL;

static void handle_signal(int signum) {
	stop_event_loop(event_loop);
}

int main(int argc, char *argv[]) {
	struct mako_state state = {0};

	init_config(&state.config);

	int ret = load_config_file(&state.config);
	if (ret < 0) {
		return EXIT_FAILURE;
	}
	ret = parse_config_arguments(&state.config, argc, argv);
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
	struct sigaction sa = { .sa_handler = handle_signal };
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	ret = run_event_loop(&state.event_loop);

	finish(&state);
	finish_config(&state.config);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
