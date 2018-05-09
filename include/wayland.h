#ifndef _MAKO_WAYLAND_H
#define _MAKO_WAYLAND_H

#include <stdbool.h>

struct mako_state;

struct mako_pointer {
	struct mako_state *state;
	struct wl_pointer *wl_pointer;
	struct wl_list link; // mako_state::pointers

	int32_t x, y;
};

bool init_wayland(struct mako_state *state);
void finish_wayland(struct mako_state *state);
void send_frame(struct mako_state *state);

#endif
