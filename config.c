#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "config.h"
#include "criteria.h"
#include "string-util.h"
#include "types.h"

static int32_t max(int32_t a, int32_t b) {
	return (a > b) ? a : b;
}

void init_default_config(struct mako_config *config) {
	wl_list_init(&config->criteria);
	struct mako_criteria *new_criteria = create_criteria(config);
	init_default_style(&new_criteria->style);
	new_criteria->raw_string = strdup("(root)");

	// Hide grouped notifications by default, and put the group count in
	// their format...
	new_criteria = create_criteria(config);
	init_empty_style(&new_criteria->style);
	new_criteria->grouped = true;
	new_criteria->spec.grouped = true;
	new_criteria->style.invisible = true;
	new_criteria->style.spec.invisible = true;
	new_criteria->style.format = strdup("(%g) <b>%s</b>\n%b");
	new_criteria->style.spec.format = true;
	new_criteria->raw_string = strdup("(default grouped)");

	// ...but make the first one in the group visible.
	new_criteria = create_criteria(config);
	init_empty_style(&new_criteria->style);
	new_criteria->group_index = 0;
	new_criteria->spec.group_index = true;
	new_criteria->style.invisible = false;
	new_criteria->style.spec.invisible = true;
	new_criteria->raw_string = strdup("(default group-index=0)");

	// Define the default format for the hidden placeholder notification.
	new_criteria = create_criteria(config);
	init_empty_style(&new_criteria->style);
	new_criteria->hidden = true;
	new_criteria->spec.hidden = true;
	new_criteria->style.format = strdup("(%h more)");
	new_criteria->style.spec.format = true;
	new_criteria->raw_string = strdup("(default hidden)");

	init_empty_style(&config->superstyle);

	config->max_history = 5;
	config->sort_criteria = MAKO_SORT_CRITERIA_TIME;
	config->sort_asc = 0;
}

void finish_config(struct mako_config *config) {
	struct mako_criteria *criteria, *tmp;
	wl_list_for_each_safe(criteria, tmp, &config->criteria, link) {
		destroy_criteria(criteria);
	}

	finish_style(&config->superstyle);
}

void init_default_style(struct mako_style *style) {
	style->width = 300;
	style->height = 100;

	style->outer_margin.top = 0;
	style->outer_margin.right = 0;
	style->outer_margin.bottom = 0;
	style->outer_margin.left = 0;

	style->margin.top = 10;
	style->margin.right = 10;
	style->margin.bottom = 10;
	style->margin.left = 10;

	style->padding.top = 5;
	style->padding.right = 5;
	style->padding.bottom = 5;
	style->padding.left = 5;

	style->border_radius.top = 0;
	style->border_radius.right = 0;
	style->border_radius.bottom = 0;
	style->border_radius.left = 0;

	style->border_size = 2;

#ifdef HAVE_ICONS
	style->icons = true;
#else
	style->icons = false;
#endif
	style->max_icon_size = 64;
	style->icon_path = strdup("");  // hicolor and pixmaps are implicit.

	style->font = strdup("monospace 10");
	style->markup = true;
	style->format = strdup("<b>%s</b>\n%b");
	style->text_alignment = PANGO_ALIGN_LEFT;

	style->actions = true;
	style->default_timeout = 0;
	style->ignore_timeout = false;

	style->colors.background = 0x285577FF;
	style->colors.text = 0xFFFFFFFF;
	style->colors.border = 0x4C7899FF;
	style->colors.progress.value = 0x5588AAFF;
	style->colors.progress.operator = CAIRO_OPERATOR_OVER;

	style->group_criteria_spec.none = true;
	style->invisible = false;
	style->history = true;
	style->icon_location = MAKO_ICON_LOCATION_LEFT;

	style->output = strdup("");
	style->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	style->max_visible = 5;

	style->anchor =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

	style->button_bindings.left.action = MAKO_BINDING_INVOKE_ACTION;
	style->button_bindings.left.action_name = strdup(DEFAULT_ACTION_KEY);
	style->button_bindings.right.action = MAKO_BINDING_DISMISS;
	style->button_bindings.middle.action = MAKO_BINDING_NONE;
	style->touch_binding.action = MAKO_BINDING_DISMISS;

	// Everything in the default config is explicitly specified.
	memset(&style->spec, true, sizeof(struct mako_style_spec));
}

void init_empty_style(struct mako_style *style) {
	memset(style, 0, sizeof(struct mako_style));
}

static void finish_binding(struct mako_binding *binding) {
	free(binding->command);
	free(binding->action_name);
}

void finish_style(struct mako_style *style) {
	finish_binding(&style->button_bindings.left);
	finish_binding(&style->button_bindings.middle);
	finish_binding(&style->button_bindings.right);
	finish_binding(&style->touch_binding);
	finish_binding(&style->notify_binding);
	free(style->icon_path);
	free(style->font);
	free(style->format);
	free(style->output);
}

static void copy_binding(struct mako_binding *dst,
		const struct mako_binding *src) {
	finish_binding(dst);

	*dst = *src;
	if (src->command != NULL) {
		dst->command = strdup(src->command);
	}
	if (src->action_name != NULL) {
		dst->action_name = strdup(src->action_name);
	}
}

// Update `target` with the values specified in `style`. If a failure occurs,
// `target` will remain unchanged.
bool apply_style(struct mako_style *target, const struct mako_style *style) {
	// Try to duplicate strings up front in case allocation fails and we have
	// to bail without changing `target`.
	char *new_font = NULL;
	char *new_format = NULL;
	char *new_icon_path = NULL;
	char *new_output = NULL;

	if (style->spec.font) {
		new_font = strdup(style->font);
		if (new_font == NULL) {
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	if (style->spec.format) {
		new_format = strdup(style->format);
		if (new_format == NULL) {
			free(new_font);
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	if (style->spec.icon_path) {
		new_icon_path = strdup(style->icon_path);
		if (new_icon_path == NULL) {
			free(new_format);
			free(new_font);
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	if (style->spec.output) {
		new_output = strdup(style->output);
		if (new_output == NULL) {
			free(new_format);
			free(new_font);
			free(new_icon_path);
			fprintf(stderr, "allocation failed\n");
			return false;
		}
	}

	// Now on to actually setting things!

	if (style->spec.width) {
		target->width = style->width;
		target->spec.width = true;
	}

	if (style->spec.height) {
		target->height = style->height;
		target->spec.height = true;
	}

	if (style->spec.outer_margin) {
		target->outer_margin = style->outer_margin;
		target->spec.outer_margin = true;
	}

	if (style->spec.margin) {
		target->margin = style->margin;
		target->spec.margin = true;
	}

	if (style->spec.padding) {
		target->padding = style->padding;
		target->spec.padding = true;
	}

	if (style->spec.border_size) {
		target->border_size = style->border_size;
		target->spec.border_size = true;
	}

	if (style->spec.icons) {
		target->icons = style->icons;
		target->spec.icons = true;
	}

	if (style->spec.max_icon_size) {
		target->max_icon_size = style->max_icon_size;
		target->spec.max_icon_size = true;
	}

	if (style->spec.icon_path) {
		free(target->icon_path);
		target->icon_path = new_icon_path;
		target->spec.icon_path = true;
	}

	if (style->spec.font) {
		free(target->font);
		target->font = new_font;
		target->spec.font = true;
	}

	if (style->spec.markup) {
		target->markup = style->markup;
		target->spec.markup = true;
	}

	if (style->spec.format) {
		free(target->format);
		target->format = new_format;
		target->spec.format = true;
	}

	if (style->spec.text_alignment) {
		target->text_alignment = style->text_alignment;
		target->spec.text_alignment = true;
	}

	if (style->spec.actions) {
		target->actions = style->actions;
		target->spec.actions = true;
	}

	if (style->spec.default_timeout) {
		target->default_timeout = style->default_timeout;
		target->spec.default_timeout = true;
	}

	if (style->spec.ignore_timeout) {
		target->ignore_timeout = style->ignore_timeout;
		target->spec.ignore_timeout = true;
	}

	if (style->spec.colors.background) {
		target->colors.background = style->colors.background;
		target->spec.colors.background = true;
	}

	if (style->spec.colors.text) {
		target->colors.text = style->colors.text;
		target->spec.colors.text = true;
	}

	if (style->spec.colors.border) {
		target->colors.border = style->colors.border;
		target->spec.colors.border = true;
	}

	if (style->spec.colors.progress) {
		target->colors.progress = style->colors.progress;
		target->spec.colors.progress = true;
	}

	if (style->spec.group_criteria_spec) {
		target->group_criteria_spec = style->group_criteria_spec;
		target->spec.group_criteria_spec = true;
	}

	if (style->spec.invisible) {
		target->invisible = style->invisible;
		target->spec.invisible = true;
	}

	if (style->spec.history) {
		target->history = style->history;
		target->spec.history = true;
	}

	if (style->spec.icon_location) {
		target->icon_location = style->icon_location;
		target->spec.icon_location = true;
	}

	if (style->spec.border_radius) {
		target->border_radius = style->border_radius;
		target->spec.border_radius = true;
	}

	if (style->spec.output) {
		free(target->output);
		target->output = new_output;
		target->spec.output = true;
	}

	if (style->spec.anchor) {
		target->anchor = style->anchor;
		target->spec.anchor = true;
	}

	if (style->spec.layer) {
		target->layer = style->layer;
		target->spec.layer = true;
	}

	if (style->spec.max_visible) {
		target->max_visible = style->max_visible;
		target->spec.max_visible = true;
	}

	if (style->spec.button_bindings.left) {
		copy_binding(&target->button_bindings.left, &style->button_bindings.left);
		target->spec.button_bindings.left = true;
	}
	if (style->spec.button_bindings.middle) {
		copy_binding(&target->button_bindings.middle, &style->button_bindings.middle);
		target->spec.button_bindings.middle = true;
	}
	if (style->spec.button_bindings.right) {
		copy_binding(&target->button_bindings.right, &style->button_bindings.right);
		target->spec.button_bindings.right = true;
	}

	if (style->spec.touch_binding) {
		copy_binding(&target->touch_binding, &style->touch_binding);
		target->spec.touch_binding = true;
	}

	if (style->spec.notify_binding) {
		copy_binding(&target->notify_binding, &style->notify_binding);
		target->spec.notify_binding = true;
	}

	return true;
}

// Given a config and a style in which to store the information, this will
// calculate a style that has the maximum value of all the configured criteria
// styles (including the default as a base), for values where it makes sense to
// have a maximum. Those that don't make sense will be unchanged. Usually, you
// want to pass an empty style as the target.
bool apply_superset_style(
		struct mako_style *target, struct mako_config *config) {
	// Specify eveything that we'll be combining.
	target->spec.width = true;
	target->spec.height = true;
	target->spec.outer_margin = true;
	target->spec.margin = true;
	target->spec.padding = true;
	target->spec.border_size = true;
	target->spec.border_radius = true;
	target->spec.icons = true;
	target->spec.max_icon_size = true;
	target->spec.default_timeout = true;
	target->spec.markup = true;
	target->spec.actions = true;
	target->spec.history = true;
	target->spec.format = true;

	free(target->format);

	// The "format" needs enough space for one of each specifier.
	target->format = calloc(1, (2 * strlen(VALID_FORMAT_SPECIFIERS)) + 1);
	char *target_format_pos = target->format;

	// Now we loop over the criteria and add together those fields.
	// We can't use apply_style, because it simply overwrites each field.
	struct mako_criteria *criteria;
	wl_list_for_each(criteria, &config->criteria, link) {
		struct mako_style *style = &criteria->style;

		// We can cheat and skip checking whether any of these are specified,
		// since we're looking for the max and unspecified ones will be
		// initialized to zero.
		target->width = max(style->width, target->width);
		target->height = max(style->height, target->height);
		target->outer_margin.top = max(style->outer_margin.top, target->outer_margin.top);
		target->outer_margin.right = max(style->outer_margin.right, target->outer_margin.right);
		target->outer_margin.bottom =
			max(style->outer_margin.bottom, target->outer_margin.bottom);
		target->outer_margin.left = max(style->outer_margin.left, target->outer_margin.left);
		target->margin.top = max(style->margin.top, target->margin.top);
		target->margin.right = max(style->margin.right, target->margin.right);
		target->margin.bottom =
			max(style->margin.bottom, target->margin.bottom);
		target->margin.left = max(style->margin.left, target->margin.left);
		target->padding.top = max(style->padding.top, target->padding.top);
		target->padding.right = max(style->padding.right, target->padding.right);
		target->padding.bottom =
			max(style->padding.bottom, target->padding.bottom);
		target->padding.left = max(style->padding.left, target->padding.left);
		target->border_radius.top = max(style->border_radius.top, target->border_radius.top);
		target->border_radius.right = max(style->border_radius.right, target->border_radius.right);
		target->border_radius.bottom =
			max(style->border_radius.bottom, target->border_radius.bottom);
		target->border_radius.left = max(style->border_radius.left, target->border_radius.left);
		target->border_size = max(style->border_size, target->border_size);
		target->icons = style->icons || target->icons;
		target->max_icon_size = max(style->max_icon_size, target->max_icon_size);
		target->default_timeout =
			max(style->default_timeout, target->default_timeout);

		target->markup |= style->markup;
		target->actions |= style->actions;
		target->history |= style->history;

		// We do need to be safe about this one though.
		if (style->spec.format) {
			char *format_pos = style->format;
			char current_specifier[3] = {0};
			while (*format_pos) {
				format_pos = strstr(format_pos, "%");
				if (!format_pos) {
					break;
				}

				// We only want to add the format specifier to the target if we
				// haven't already seen it.
				// Need to copy the specifier into its own string to use strstr
				// here, because there's no way to limit how much of the string
				// it uses in the comparison.
				memcpy(&current_specifier, format_pos, 2);
				if (!strstr(target->format, current_specifier)) {
					memcpy(target_format_pos, format_pos, 2);
					target_format_pos += 2; // This needs to go to the next slot.
				}

				++format_pos; // Enough to move to the next match.
			}
		}
	}

	return true;
}

static char *expand_config_path(const char *path) {
	if (strncmp(path, "/", 1) == 0) {
		return strdup(path);
	}

	if (strncmp(path, "~/", 2) != 0) {
		fprintf(stderr, "Config path must start with / or ~/\n");
		return NULL;
	}

	const char *home = getenv("HOME");
	if (home == NULL) {
		fprintf(stderr, "HOME env var not set\n");
		return NULL;
	}

	return mako_asprintf("%s/%s", home, path + 2);
}

static bool apply_config_option(struct mako_config *config, const char *name,
		const char *value) {
	if (strcmp(name, "sort") == 0) {
		if (strcmp(value, "+priority") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_URGENCY;
			config->sort_asc |= MAKO_SORT_CRITERIA_URGENCY;
		} else if (strcmp(value, "-priority") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_URGENCY;
			config->sort_asc &= ~MAKO_SORT_CRITERIA_URGENCY;
		} else if (strcmp(value, "+time") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_TIME;
			config->sort_asc |= MAKO_SORT_CRITERIA_TIME;
		} else if (strcmp(value, "-time") == 0) {
			config->sort_criteria |= MAKO_SORT_CRITERIA_TIME;
			config->sort_asc &= ~MAKO_SORT_CRITERIA_TIME;
		} else {
			return false;
		}
		return true;
	} else if (strcmp(name, "max-history") == 0) {
		return parse_int(value, &config->max_history);
	} else if (strcmp(name, "include") == 0) {
		char *path = expand_config_path(value);
		return path && load_config_file(config, path) == 0;
	}

	return false;
}

static bool has_prefix(const char *str, const char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static bool apply_style_option(struct mako_style *style, const char *name,
		const char *value) {
	struct mako_style_spec *spec = &style->spec;

	if (strcmp(name, "font") == 0) {
		free(style->font);
		return spec->font = !!(style->font = strdup(value));
	} else if (strcmp(name, "background-color") == 0) {
		return spec->colors.background =
			parse_color(value, &style->colors.background);
	} else if (strcmp(name, "text-color") == 0) {
		return spec->colors.text = parse_color(value, &style->colors.text);
	} else if (strcmp(name, "width") == 0) {
		return spec->width = parse_int_ge(value, &style->width, 1);
	} else if (strcmp(name, "height") == 0) {
		return spec->height = parse_int_ge(value, &style->height, 1);
	} else if (strcmp(name, "outer-margin") == 0) {
		return spec->outer_margin = parse_directional(value, &style->outer_margin);
	} else if (strcmp(name, "margin") == 0) {
		return spec->margin = parse_directional(value, &style->margin);
	} else if (strcmp(name, "padding") == 0) {
		spec->padding = parse_directional(value, &style->padding);
		if (spec->border_radius && spec->padding) {
			style->padding.left = max(style->border_radius.left, style->padding.left);
			style->padding.right = max(style->border_radius.right, style->padding.right);
		}
		return spec->padding;
	} else if (strcmp(name, "border-size") == 0) {
		return spec->border_size = parse_int_ge(value, &style->border_size, 0);
	} else if (strcmp(name, "border-color") == 0) {
		return spec->colors.border = parse_color(value, &style->colors.border);
	} else if (strcmp(name, "progress-color") == 0) {
		return spec->colors.progress = parse_mako_color(value, &style->colors.progress);
	} else if (strcmp(name, "icons") == 0) {
#ifdef HAVE_ICONS
		return spec->icons =
			parse_boolean(value, &style->icons);
#else
		fprintf(stderr, "Icon support not built in, ignoring icons setting.\n");
		return true;
#endif
	} else if (strcmp(name, "icon-location") == 0) {
		if (!strcmp(value, "left")) {
			style->icon_location = MAKO_ICON_LOCATION_LEFT;
		} else if (!strcmp(value, "right")) {
			style->icon_location = MAKO_ICON_LOCATION_RIGHT;
		} else if (!strcmp(value, "top")) {
			style->icon_location = MAKO_ICON_LOCATION_TOP;
		} else if (!strcmp(value, "bottom")) {
			style->icon_location = MAKO_ICON_LOCATION_BOTTOM;
		} else {
			return false;
		}
		return spec->icon_location = true;
	} else if (strcmp(name, "max-icon-size") == 0) {
		return spec->max_icon_size =
			parse_int_ge(value, &style->max_icon_size, 1);
	} else if (strcmp(name, "icon-path") == 0) {
		free(style->icon_path);
		return spec->icon_path = !!(style->icon_path = strdup(value));
	} else if (strcmp(name, "markup") == 0) {
		return spec->markup = parse_boolean(value, &style->markup);
	} else if (strcmp(name, "actions") == 0) {
		return spec->actions = parse_boolean(value, &style->actions);
	} else if (strcmp(name, "format") == 0) {
		free(style->format);
		return spec->format = parse_format(value, &style->format);
	} else if (strcmp(name, "text-alignment") == 0) {
		if (strcmp(value, "left") == 0) {
			style->text_alignment = PANGO_ALIGN_LEFT;
		} else if (strcmp(value, "center") == 0) {
			style->text_alignment = PANGO_ALIGN_CENTER;
		} else if (strcmp(value, "right") == 0) {
			style->text_alignment = PANGO_ALIGN_RIGHT;
		} else {
			return false;
		}
		style->spec.text_alignment = true;
		return true;
	} else if (strcmp(name, "default-timeout") == 0) {
		return spec->default_timeout =
			parse_int_ge(value, &style->default_timeout, 0);
	} else if (strcmp(name, "ignore-timeout") == 0) {
		return spec->ignore_timeout =
			parse_boolean(value, &style->ignore_timeout);
	} else if (strcmp(name, "group-by") == 0) {
		return spec->group_criteria_spec =
			parse_criteria_spec(value, &style->group_criteria_spec);
	} else if (strcmp(name, "invisible") == 0) {
		return spec->invisible = parse_boolean(value, &style->invisible);
	} else if (strcmp(name, "history") == 0) {
		return spec->history = parse_boolean(value, &style->history);
	} else if (strcmp(name, "border-radius") == 0) {
		spec->border_radius = parse_directional(value, &style->border_radius);
		if (spec->border_radius && spec->padding) {
			style->padding.left = max(style->border_radius.left, style->padding.left);
			style->padding.right = max(style->border_radius.right, style->padding.right);
		}
		return spec->border_radius;
	} else if (strcmp(name, "max-visible") == 0) {
		return style->spec.max_visible = parse_int(value, &style->max_visible);
	} else if (strcmp(name, "output") == 0) {
		free(style->output);
		style->output = strdup(value);
		style->spec.output = true;
		return true;
	} else if (strcmp(name, "layer") == 0) {
		if (strcmp(value, "background") == 0) {
			style->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
		} else if (strcmp(value, "bottom") == 0) {
			style->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
		} else if (strcmp(value, "top") == 0) {
			style->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		} else if (strcmp(value, "overlay") == 0) {
			style->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
		} else {
			return false;
		}
		style->spec.layer = true;
		return true;
	} else if (strcmp(name, "anchor") == 0) {
		return spec->anchor = parse_anchor(value, &style->anchor);
	} else if (has_prefix(name, "on-")) {
		struct mako_binding binding = {0};
		if (strcmp(value, "none") == 0) {
			binding.action = MAKO_BINDING_NONE;
		} else if (strcmp(value, "dismiss") == 0) {
			binding.action = MAKO_BINDING_DISMISS;
		} else if (strcmp(value, "dismiss --no-history") == 0) {
			binding.action = MAKO_BINDING_DISMISS_NO_HISTORY;
		} else if (strcmp(value, "dismiss-all") == 0) {
			binding.action = MAKO_BINDING_DISMISS_ALL;
		} else if (strcmp(value, "dismiss-group") == 0) {
			binding.action = MAKO_BINDING_DISMISS_GROUP;
		} else if (strcmp(value, "invoke-default-action") == 0) {
			binding.action = MAKO_BINDING_INVOKE_ACTION;
			binding.action_name = strdup(DEFAULT_ACTION_KEY);
		} else if (has_prefix(value, "invoke-action ")) {
			binding.action = MAKO_BINDING_INVOKE_ACTION;
			binding.action_name = strdup(value + strlen("invoke-action "));
		} else if (has_prefix(value, "exec ")) {
			binding.action = MAKO_BINDING_EXEC;
			binding.command = strdup(value + strlen("exec "));
		} else {
			return false;
		}

		if (strcmp(name, "on-button-left") == 0) {
			copy_binding(&style->button_bindings.left, &binding);
			style->spec.button_bindings.left = true;
		} else if (strcmp(name, "on-button-right") == 0) {
			copy_binding(&style->button_bindings.right, &binding);
			style->spec.button_bindings.right = true;
		} else if (strcmp(name, "on-button-middle") == 0) {
			copy_binding(&style->button_bindings.middle, &binding);
			style->spec.button_bindings.middle = true;
		} else if (strcmp(name, "on-touch") == 0) {
			copy_binding(&style->touch_binding, &binding);
			style->spec.touch_binding = true;
		} else if (strcmp(name, "on-notify") == 0) {
			copy_binding(&style->notify_binding, &binding);
			style->spec.notify_binding = true;
		} else {
			return false;
		}

		return true;
	}

	return false;
}

bool apply_global_option(struct mako_config *config, const char *name,
		const char *value) {
	struct mako_criteria *global = global_criteria(config);
	return apply_style_option(&global->style, name, value) ||
		apply_config_option(config, name, value);
}

static bool file_exists(const char *path) {
	return path && access(path, R_OK) != -1;
}

static char *get_default_config_path() {
	const char *home = getenv("HOME");
	if (home == NULL) {
		fprintf(stderr, "HOME env var not set\n");
		return NULL;
	}

	const char *config_home = getenv("XDG_CONFIG_HOME");
	char *config_home_fallback = NULL;
	if (config_home == NULL || config_home[0] == '\0') {
		config_home_fallback = mako_asprintf("%s/.config", home);
		config_home = config_home_fallback;
	}

	char *config_paths[] = {
		mako_asprintf("%s/.mako/config", home),
		mako_asprintf("%s/mako/config", config_home),
	};

	size_t config_paths_len = sizeof(config_paths) / sizeof(config_paths[0]);
	char *found_path = NULL;
	for (size_t i = 0; i < config_paths_len; ++i) {
		char *path = config_paths[i];
		if (file_exists(path)) {
			found_path = strdup(path);
			break;
		}
	}

	for (size_t i = 0; i < config_paths_len; ++i) {
		free(config_paths[i]);
	}
	free(config_home_fallback);

	return found_path;
}

int load_config_file(struct mako_config *config, char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading\n", path);
		free(path);
		return -1;
	}
	const char *base = basename(path);

	int ret = 0;
	int lineno = 0;
	char *line = NULL;
	char *section = NULL;

	// Until we hit the first criteria section, we want to be modifying the
	// root criteria's style. We know it's always the first one in the list.
	struct mako_criteria *criteria =
		wl_container_of(config->criteria.next, criteria, link);

	size_t n = 0;
	while (getline(&line, &n, f) > 0) {
		++lineno;
		if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') {
			continue;
		}

		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		if (line[0] == '[' && line[strlen(line) - 1] == ']') {
			// Since we hit the end of the previous criteria section, validate
			// that it doesn't break any rules before moving on.
			if (criteria != NULL && !validate_criteria(criteria)) {
				fprintf(stderr, "Invalid configuration in criteria: [%s]\n",
						criteria->raw_string);
				ret = -1;
				break;
			}

			free(section);
			section = strndup(line + 1, strlen(line) - 2);
			criteria = create_criteria(config);
			if (!parse_criteria(section, criteria)) {
				fprintf(stderr, "[%s:%d] Invalid criteria definition\n", base,
						lineno);
				ret = -1;
				break;
			}
			continue;
		}

		char *eq = strchr(line, '=');
		if (!eq) {
			fprintf(stderr, "[%s:%d] Expected key=value\n", base, lineno);
			ret = -1;
			break;
		}

		bool valid_option = false;
		eq[0] = '\0';

		valid_option = apply_style_option(&criteria->style, line, eq + 1);

		if (!valid_option && section == NULL) {
			valid_option = apply_config_option(config, line, eq + 1);
		}

		if (!valid_option) {
			fprintf(stderr, "[%s:%d] Failed to parse option '%s'\n",
				base, lineno, line);
			ret = -1;
			break;
		}
	}

	// Validate the final criteria section since there was no opening bracket
	// after it to do this in the loop.
	if (ret != -1 && criteria != NULL && !validate_criteria(criteria)) {
		fprintf(stderr, "Invalid configuration in criteria: [%s]\n",
				criteria->raw_string);
		ret = -1;
	}

	free(section);
	free(line);
	fclose(f);
	free(path);
	return ret;
}

int parse_config_arguments(struct mako_config *config, int argc, char **argv) {
	static const struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"config", required_argument, 0, 'c'},
		{"font", required_argument, 0, 0},
		{"background-color", required_argument, 0, 0},
		{"text-color", required_argument, 0, 0},
		{"width", required_argument, 0, 0},
		{"height", required_argument, 0, 0},
		{"outer-margin", required_argument, 0, 0},
		{"margin", required_argument, 0, 0},
		{"padding", required_argument, 0, 0},
		{"border-size", required_argument, 0, 0},
		{"border-color", required_argument, 0, 0},
		{"border-radius", required_argument, 0, 0},
		{"progress-color", required_argument, 0, 0},
		{"icons", required_argument, 0, 0},
		{"icon-location", required_argument, 0, 0},
		{"icon-path", required_argument, 0, 0},
		{"max-icon-size", required_argument, 0, 0},
		{"markup", required_argument, 0, 0},
		{"actions", required_argument, 0, 0},
		{"format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{"max-history", required_argument, 0, 0},
		{"history", required_argument, 0, 0},
		{"default-timeout", required_argument, 0, 0},
		{"ignore-timeout", required_argument, 0, 0},
		{"output", required_argument, 0, 0},
		{"layer", required_argument, 0, 0},
		{"anchor", required_argument, 0, 0},
		{"sort", required_argument, 0, 0},
		{"group-by", required_argument, 0, 0},
		{"on-button-left", required_argument, 0, 0},
		{"on-button-right", required_argument, 0, 0},
		{"on-button-middle", required_argument, 0, 0},
		{"on-touch", required_argument, 0, 0},
		{0},
	};

	optind = 1;
	char *config_arg = NULL;
	int opt_status = 0;
	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "hc:", long_options, &option_index);
		if (c < 0) {
			break;
		} else if (c == 'h') {
			opt_status = 1;
			break;
		} else if (c == 'c') {
			free(config_arg);
			config_arg = strdup(optarg);
		} else if (c != 0) {
			opt_status = -1;
			break;
		}
	}

	if (opt_status != 0) {
		free(config_arg);
		return opt_status;
	}

	char *config_path = config_arg ? config_arg : get_default_config_path();
	if (config_path) {
		int config_status = load_config_file(config, config_path);
		if (config_status < 0) {
			return -1;
		}
	}

	optind = 1;
	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "hc:", long_options, &option_index);
		if (c < 0) {
			break;
		} else if (c == 'h' || c == 'c') {
			continue;
		} else if (c != 0) {
			return -1;
		}

		const char *name = long_options[option_index].name;
		if (!apply_global_option(config, name, optarg)) {
			fprintf(stderr, "Failed to parse option '%s'\n", name);
			return -1;
		}
	}

	return 0;
}

// Returns zero on success, negative on error, positive if we should exit
// immediately due to something the user asked for (like help).
int reload_config(struct mako_config *config, int argc, char **argv) {
	struct mako_config new_config = {0};
	init_default_config(&new_config);

	int args_status = parse_config_arguments(&new_config, argc, argv);

	if (args_status > 0) {
		finish_config(&new_config);
		return args_status;
	} else if (args_status < 0) {
		fprintf(stderr, "Failed to parse config\n");
		finish_config(&new_config);
		return -1;
	}

	apply_superset_style(&new_config.superstyle, &new_config);

	finish_config(config);
	*config = new_config;

	// We have to rebuild the wl_list that contains the criteria, as it is
	// currently pointing to local memory instead of the location of the real
	// criteria struct.
	wl_list_init(&config->criteria);
	wl_list_insert_list(&config->criteria, &new_config.criteria);

	return 0;
}
