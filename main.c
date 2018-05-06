#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus.h"
#include "mako.h"
#include "render.h"
#include "wayland.h"

struct mako_notification *create_notification(struct mako_state *state) {
	struct mako_notification *notif =
		calloc(1, sizeof(struct mako_notification));
	if (notif == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}
	notif->state = state;
	return notif;
}

void insert_notification(struct mako_notification *notif) {
	struct mako_state *state = notif->state;

	if (!init_wayland_notification(notif)) {
		return;
	}

	++state->last_id;
	notif->id = state->last_id;
	wl_list_insert(&state->notifications, &notif->link);
}

void destroy_notification(struct mako_notification *notif) {
	wl_list_remove(&notif->link);
	finish_wayland_notification(notif);
	free(notif->app_name);
	free(notif->app_icon);
	free(notif->summary);
	free(notif->body);
	free(notif);
}

static bool init(struct mako_state *state) {
	if (!init_dbus(state)) {
		return false;
	}
	if (!init_wayland(state)) {
		finish_dbus(state);
		return false;
	}
	wl_list_init(&state->notifications);
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

	int ret = 0;
	while (1) {
		while (wl_display_prepare_read(state.display) != 0) {
			wl_display_dispatch_pending(state.display);
		}
		wl_display_flush(state.display);

		ret = poll(fds, 2, -1);
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

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
