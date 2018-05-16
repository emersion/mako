#include <errno.h>
#include <stdio.h>

#include "event-loop.h"

void init_event_loop(struct mako_event_loop *loop, sd_bus *bus,
		struct wl_display *display) {
	loop->fds[MAKO_EVENT_DBUS] = (struct pollfd){
		.fd = sd_bus_get_fd(bus),
		.events = POLLIN,
	};

	loop->fds[MAKO_EVENT_WAYLAND] = (struct pollfd){
		.fd = wl_display_get_fd(display),
		.events = POLLIN,
	};

	loop->bus = bus;
	loop->display = display;
}

void finish_event_loop(struct mako_event_loop *loop) {
	// No-op
}

static int poll_event_loop(struct mako_event_loop *loop) {
	return poll(loop->fds, MAKO_EVENT_COUNT, -1);
}

int run_event_loop(struct mako_event_loop *loop) {
	loop->running = true;

	int ret = 0;
	while (loop->running) {
		while (wl_display_prepare_read(loop->display) != 0) {
			wl_display_dispatch_pending(loop->display);
		}
		wl_display_flush(loop->display);

		ret = poll_event_loop(loop);
		if (ret < 0) {
			wl_display_cancel_read(loop->display);
			fprintf(stderr, "failed to poll(): %s\n", strerror(-ret));
			break;
		}

		if (!(loop->fds[MAKO_EVENT_WAYLAND].revents & POLLIN)) {
			wl_display_cancel_read(loop->display);
		}

		if (loop->fds[MAKO_EVENT_DBUS].revents & POLLIN) {
			while (1) {
				ret = sd_bus_process(loop->bus, NULL);
				if (ret < 0) {
					fprintf(stderr, "failed to process bus: %s\n",
						strerror(-ret));
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

		if (loop->fds[MAKO_EVENT_WAYLAND].revents & POLLIN) {
			ret = wl_display_read_events(loop->display);
			if (ret < 0) {
				fprintf(stderr, "failed to read Wayland events: %s\n",
					strerror(errno));
				break;
			}
			wl_display_dispatch_pending(loop->display);
		}
	}
	return ret;
}
