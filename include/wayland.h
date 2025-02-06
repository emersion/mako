#ifndef MAKO_WAYLAND_H
#define MAKO_WAYLAND_H

#include <stdbool.h>
#include <wayland-client-protocol.h>
#include "mako.h"

#define MAX_TOUCHPOINTS 10

struct mako_state;
struct mako_surface;

struct mako_output {
	struct mako_state *state;
	uint32_t global_name;
	struct wl_output *wl_output;
	struct wl_list link; // mako_state::outputs

	char *name;
	enum wl_output_subpixel subpixel;
	int32_t scale;
};

struct mako_seat {
	struct mako_state *state;
	struct wl_seat *wl_seat;
	struct wl_list link; // mako_state::seats

	struct {
		struct wl_pointer *wl_pointer;
		int32_t x, y;
		struct mako_surface *surface;
	} pointer;

	struct {
		struct wl_touch *wl_touch;
		struct {
			int32_t x, y;
			uint32_t time;
			struct mako_surface *surface;
		} pts[MAX_TOUCHPOINTS];
	} touch;
};

bool init_wayland(struct mako_state *state);
void finish_wayland(struct mako_state *state);
void set_dirty(struct mako_surface *surface);
char *create_xdg_activation_token(struct mako_surface *surface,
	struct mako_seat *seat, uint32_t serial);

#endif
