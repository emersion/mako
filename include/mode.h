#ifndef MODE_H
#define MODE_H

#include <stdbool.h>

struct mako_state;

bool has_mode(struct mako_state *state, const char *mode);
void set_modes(struct mako_state *state, const char **modes, size_t modes_len);

#endif
