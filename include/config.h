#ifndef _MAKO_CONFIG_H
#define _MAKO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

struct mako_directional {
	int32_t top;
	int32_t right;
	int32_t bottom;
	int32_t left;
};

enum mako_button_binding {
	MAKO_BUTTON_BINDING_NONE,
	MAKO_BUTTON_BINDING_DISMISS,
	MAKO_BUTTON_BINDING_DISMISS_ALL,
	MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION,
};

enum mako_sort_criteria {
	MAKO_SORT_CRITERIA_TIME = 1,
	MAKO_SORT_CRITERIA_URGENCY = 2,
	MAKO_SORT_ASC_TIME = 1,
	MAKO_SORT_ASC_URGENCY = 2,
};

struct mako_config {
	char *font;
	int32_t width, height;
	int32_t padding;
	int32_t border_size;
	bool markup;
	char *format;
	bool actions;
	struct mako_directional margin;
	int32_t max_visible;
	char *output;
	char *hidden_format;
	uint32_t sort_criteria; //enum mako_sort_criteria
	uint32_t sort_asc;

	int default_timeout; // in ms

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
int load_config_file(struct mako_config *config);
void reload_config(struct mako_config *config);

#endif
