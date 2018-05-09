#ifndef _MAKO_HOTSPOT_H
#define _MAKO_HOTSPOT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

struct mako_hotspot {
	int32_t x, y;
	int32_t width, height;
	void (*handle_button)(void *user_data, uint32_t button,
		enum wl_pointer_button_state state);
	void *user_data;
};

bool hotspot_at(struct mako_hotspot *hotspot, int32_t x, int32_t y);
void hotspot_send_button(struct mako_hotspot *hotspot, uint32_t button,
	enum wl_pointer_button_state state);

#endif
