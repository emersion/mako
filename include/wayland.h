#ifndef _MAKO_WAYLAND_H
#define _MAKO_WAYLAND_H

#include <stdbool.h>

struct mako_state;

bool init_wayland(struct mako_state *state);
void finish_wayland(struct mako_state *state);

#endif
