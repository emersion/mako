#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <poll.h>
#include <stdbool.h>
#include <time.h>
#include <wayland-client.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_SUBD
#include "subd.h"
#endif

enum mako_event {
#if defined(HAVE_SYSTEMD) || defined(HAVE_ELOGIND)
	MAKO_EVENT_DBUS,
#endif
	MAKO_EVENT_WAYLAND,
	MAKO_EVENT_TIMER,
	MAKO_EVENT_SIGNAL,
	MAKO_EVENT_COUNT, // keep last
};

struct mako_event_loop {
#if defined(HAVE_SYSTEMD) || defined(HAVE_ELOGIND)
	struct pollfd fds[MAKO_EVENT_COUNT];
#else
	struct subd_watches *watches;
	struct pollfd *fds;
#endif
	sd_bus *bus;
	struct wl_display *display;
	int sfd;

	bool running;
	struct wl_list timers; // mako_timer::link
	struct mako_timer *next_timer;
};

typedef void (*mako_event_loop_timer_func_t)(void *data);

struct mako_timer {
	struct mako_event_loop *event_loop;
	mako_event_loop_timer_func_t func;
	void *user_data;
	struct timespec at;
	struct wl_list link; // mako_event_loop::timers
};

bool init_event_loop(struct mako_event_loop *loop, sd_bus *bus,
	struct wl_display *display);
void finish_event_loop(struct mako_event_loop *loop);
int run_event_loop(struct mako_event_loop *loop);
struct mako_timer *add_event_loop_timer(struct mako_event_loop *loop,
	int delay_ms, mako_event_loop_timer_func_t func, void *data);

void destroy_timer(struct mako_timer *timer);

#endif
