#ifndef _MAKO_CONFIG_H
#define _MAKO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "types.h"

enum mako_button_binding {
	MAKO_BUTTON_BINDING_NONE,
	MAKO_BUTTON_BINDING_DISMISS,
	MAKO_BUTTON_BINDING_DISMISS_ALL,
	MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION,
};

enum mako_sort_criteria {
	MAKO_SORT_CRITERIA_TIME = 1,
	MAKO_SORT_CRITERIA_URGENCY = 2,
};

// Represents which fields in the style were specified in this style. All
// fields in the mako_style structure should have a counterpart here. Inline
// structs are also mirrored.
struct mako_style_spec {
	bool width, height, margin, padding, border_size, font, markup, format,
		 actions, default_timeout, ignore_timeout;

	struct {
		bool background, text, border;
	} colors;
};

struct mako_style {
	struct mako_style_spec spec;

	int32_t width;
	int32_t height;
	struct mako_directional margin;
	int32_t padding;
	int32_t border_size;

	char *font;
	bool markup;
	char *format;

	bool actions;
	int default_timeout; // in ms
	bool ignore_timeout;

	struct {
		uint32_t background;
		uint32_t text;
		uint32_t border;
	} colors;
};

struct mako_config {
	struct wl_list criteria; // mako_criteria::link

	int32_t max_visible;
	char *output;
	uint32_t anchor;
	uint32_t sort_criteria; //enum mako_sort_criteria
	uint32_t sort_asc;

	struct mako_style hidden_style;
	struct mako_style superstyle;

	struct {
		enum mako_button_binding left, right, middle;
	} button_bindings;
};

void init_default_config(struct mako_config *config);
void finish_config(struct mako_config *config);

void init_default_style(struct mako_style *style);
void init_empty_style(struct mako_style *style);
void finish_style(struct mako_style *style);
bool apply_style(struct mako_style *target, const struct mako_style *style);
bool apply_superset_style(
		struct mako_style *target, struct mako_config *config);

int parse_config_arguments(struct mako_config *config, int argc, char **argv);
int load_config_file(struct mako_config *config);
int reload_config(struct mako_config *config, int argc, char **argv);

#endif
