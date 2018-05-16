#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <poll.h>
#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <wayland-client.h>

enum mako_event {
	MAKO_EVENT_DBUS,
	MAKO_EVENT_WAYLAND,
	MAKO_EVENT_COUNT, // keep last
};

struct mako_event_loop {
	struct pollfd fds[MAKO_EVENT_COUNT];
	sd_bus *bus;
	struct wl_display *display;

	bool running;
};

void init_event_loop(struct mako_event_loop *loop, sd_bus *bus,
	struct wl_display *display);
void finish_event_loop(struct mako_event_loop *loop);
int run_event_loop(struct mako_event_loop *loop);

#endif
