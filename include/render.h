#ifndef _MAKO_RENDER_H
#define _MAKO_RENDER_H

struct mako_state;

int render(struct mako_state *state, struct pool_buffer *buffer, int scale);

#endif
