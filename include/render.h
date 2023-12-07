#ifndef MAKO_RENDER_H
#define MAKO_RENDER_H

struct mako_state;
struct mako_surface;

void render(struct mako_surface *surface, struct pool_buffer *buffer, int scale,
	int *width, int *height);

#endif
