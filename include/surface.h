#ifndef MAKO_SURFACE_H
#define MAKO_SURFACE_H

#include "config.h"

struct mako_state;
struct mako_surface;

void destroy_surface(struct mako_surface *surface);
struct mako_surface *create_surface(struct mako_state *state, const char *output,
		enum zwlr_layer_shell_v1_layer layer, uint32_t anchor,
		int32_t max_visible);
#endif
