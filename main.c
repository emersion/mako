#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbus.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"

const char *usage =
	"Usage: mako [options...]\n"
	"\n"
	"  -h, --help                     Show help message and quit.\n"
	"      --font <font>              Font family and size.\n"
	"      --background-color <color> Background color.\n"
	"      --text-color <color>       Text color.\n"
	"      --width <px>               Notification width.\n"
	"      --height <px>              Max notification height.\n"
	"      --margin <px>              Margin.\n"
	"      --padding <px>             Padding.\n"
	"      --border-size <px>         Border size.\n"
	"      --border-color <color>     Border color.\n"
	"      --markup <0|1>             Enable/disable markup.\n"
	"      --format <format>          Format string.\n"
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
	wl_list_init(&state->notifications);
	state->running = true;
	return true;
}

static void finish(struct mako_state *state) {
	finish_dbus(state);
	finish_wayland(state);
	// TODO: finish_render(state)
	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		destroy_notification(notif);
	}
}

int main(int argc, char *argv[]) {
	struct mako_state state = { 0 };

	init_config(&state.config);

	int ret = parse_config_arguments(&state.config, argc, argv);
	if (ret < 0) {
		return EXIT_FAILURE;
	} else if (ret > 0) {
		printf("%s", usage);
		return EXIT_SUCCESS;
	}

	if (!init(&state)) {
		return EXIT_FAILURE;
	}

	struct pollfd fds[2] = {
		{
			.fd = sd_bus_get_fd(state.bus),
			.events = POLLIN,
		},
		{
			.fd = wl_display_get_fd(state.display),
			.events = POLLIN,
		},
	};
	size_t fds_len = sizeof(fds) / sizeof(fds[0]);

	while (state.running) {
		while (wl_display_prepare_read(state.display) != 0) {
			wl_display_dispatch_pending(state.display);
		}
		wl_display_flush(state.display);

		ret = poll(fds, fds_len, -1);
		if (ret < 0) {
			wl_display_cancel_read(state.display);
			fprintf(stderr, "failed to poll(): %s\n", strerror(-ret));
			break;
		}

		if (!(fds[1].revents & POLLIN)) {
			wl_display_cancel_read(state.display);
		}

		if (fds[0].revents & POLLIN) {
			while (1) {
				ret = sd_bus_process(state.bus, NULL);
				if (ret < 0) {
					fprintf(stderr, "failed to process bus: %s\n", strerror(-ret));
					break;
				}
				if (ret == 0) {
					break;
				}
				// We processed a request, try to process another one, right-away
			}

			if (ret < 0) {
				break;
			}
		}

		if (fds[1].revents & POLLIN) {
			ret = wl_display_read_events(state.display);
			if (ret < 0) {
				fprintf(stderr, "failed to read Wayland events: %s\n", strerror(errno));
				break;
			}
			wl_display_dispatch_pending(state.display);
		}
	}

	finish(&state);
	finish_config(&state.config);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
