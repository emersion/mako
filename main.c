#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"

struct mako_state mako = { 0 };

int main(int argc, char *argv[]) {
	if (!init_dbus(&mako)) {
		return EXIT_FAILURE;
	}
	if (!init_wayland(&mako)) {
		finish_dbus(&mako);
		return EXIT_FAILURE;
	}

	struct pollfd fds[2];
	fds[0].fd = sd_bus_get_fd(mako.bus);
	fds[0].events = POLLIN;
	fds[1].fd = wl_display_get_fd(mako.display);
	fds[1].events = POLLIN;

	int ret = 0;
	while (1) {
		ret = poll(fds, 2, -1);
		if (ret < 0) {
			fprintf(stderr, "Failed to poll(): %s\n", strerror(-ret));
			break;
		}

		if (fds[0].revents & POLLIN) {
			while (1) {
				ret = sd_bus_process(mako.bus, NULL);
				if (ret < 0) {
					fprintf(stderr, "Failed to process bus: %s\n", strerror(-ret));
					break;
				}
				if (ret == 0) {
					break;
				}
				// We processed a request, try to process another one, right-away
			}
		}

		if (fds[1].revents & POLLIN) {
			ret = wl_display_dispatch(mako.display);
			if (ret < 0) {
				fprintf(stderr, "Failed to dispatch display: %s\n", strerror(-ret));
				break;
			}
		}
	}

	finish_dbus(&mako);
	finish_wayland(&mako);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
