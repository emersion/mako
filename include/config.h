#ifndef _MAKO_CONFIG_H
#define _MAKO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

enum mako_button_binding {
	MAKO_BUTTON_BINDING_NONE,
	MAKO_BUTTON_BINDING_DISMISS,
	MAKO_BUTTON_BINDING_DISMISS_ALL,
	MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION,
};

struct mako_config {
	char *font;
	int32_t width, height;
	int32_t margin, padding;
	int32_t border_size;
	bool markup;
	char *format;
	bool actions;
	int32_t max_visible;

	struct {
		uint32_t background;
		uint32_t text;
		uint32_t border;
	} colors;

	struct {
		enum mako_button_binding left, right, middle;
	} button_bindings;
};

void init_config(struct mako_config *config);
void finish_config(struct mako_config *config);
int parse_config_arguments(struct mako_config *config, int argc, char **argv);

#endif
