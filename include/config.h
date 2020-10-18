#ifndef MAKO_CONFIG_H
#define MAKO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "types.h"

enum mako_binding {
	MAKO_BINDING_NONE,
	MAKO_BINDING_DISMISS,
	MAKO_BINDING_DISMISS_GROUP,
	MAKO_BINDING_DISMISS_ALL,
	MAKO_BINDING_INVOKE_DEFAULT_ACTION,
};

enum mako_sort_criteria {
	MAKO_SORT_CRITERIA_TIME = 1,
	MAKO_SORT_CRITERIA_URGENCY = 2,
};

enum mako_icon_location {
	MAKO_ICON_LOCATION_LEFT,
	MAKO_ICON_LOCATION_RIGHT,
	MAKO_ICON_LOCATION_TOP,
	MAKO_ICON_LOCATION_BOTTOM,
};

// Represents which fields in the style were specified in this style. All
// fields in the mako_style structure should have a counterpart here. Inline
// structs are also mirrored.
struct mako_style_spec {
	bool width, height, margin, padding, border_size, border_radius, font,
		markup, format, actions, default_timeout, ignore_timeout, icons,
		max_icon_size, icon_path, group_criteria_spec, invisible, history,
		icon_location, max_visible, layer, output, anchor;
	struct {
		bool background, text, border, progress;
	} colors;
};


struct mako_style {
	struct mako_style_spec spec;

	int32_t width;
	int32_t height;
	struct mako_directional margin;
	struct mako_directional padding;
	int32_t border_size;
	int32_t border_radius;

	bool icons;
	int32_t max_icon_size;
	char *icon_path;

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
		struct mako_color progress;
	} colors;

	struct mako_criteria_spec group_criteria_spec;

	bool invisible; // Skipped during render, doesn't count toward max_visible
	bool history;
	enum mako_icon_location icon_location;

	int32_t max_visible;
	char *output;
	enum zwlr_layer_shell_v1_layer layer;
	uint32_t anchor;
};

struct mako_config {
	struct wl_list criteria; // mako_criteria::link

	uint32_t sort_criteria; //enum mako_sort_criteria
	uint32_t sort_asc;
	int32_t max_history;

	struct mako_style hidden_style;
	struct mako_style superstyle;

	struct {
		enum mako_binding left, right, middle;
	} button_bindings;

	enum mako_binding touch;
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
int load_config_file(struct mako_config *config, char *config_arg);
int reload_config(struct mako_config *config, int argc, char **argv);
bool apply_global_option(struct mako_config *config, const char *name,
	const char *value);

#endif
