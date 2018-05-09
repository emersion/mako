#include "hotspot.h"

bool hotspot_at(struct mako_hotspot *hotspot, int32_t x, int32_t y) {
	return x >= hotspot->x &&
		y >= hotspot->y &&
		x < hotspot->x + hotspot->width &&
		y < hotspot->y + hotspot->height;
}

void hotspot_send_button(struct mako_hotspot *hotspot, uint32_t button,
		enum wl_pointer_button_state state) {
	if (!hotspot->handle_button) {
		return;
	}
	hotspot->handle_button(hotspot->user_data, button, state);
}
