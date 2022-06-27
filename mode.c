#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>

#include "mako.h"
#include "mode.h"

bool has_mode(struct mako_state *state, const char *mode) {
	const char **mode_ptr;
	wl_array_for_each(mode_ptr, &state->current_modes) {
		if (strcmp(*mode_ptr, mode) == 0) {
			return true;
		}
	}
	return false;
}

void set_modes(struct mako_state *state, const char **modes, size_t modes_len) {
	char **mode_ptr;
	wl_array_for_each(mode_ptr, &state->current_modes) {
		free(*mode_ptr);
	}
	state->current_modes.size = 0;

	for (size_t i = 0; i < modes_len; i++) {
		char **dst = wl_array_add(&state->current_modes, sizeof(char *));
		*dst = strdup(modes[i]);
	}
}
