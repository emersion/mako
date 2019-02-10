#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "config.h"
#include "criteria.h"
#include "types.h"


static int32_t max(int32_t a, int32_t b) {
	return (a > b) ? a : b;
}


void init_default_config(struct mako_config *config) {
	wl_list_init(&config->criteria);
	struct mako_criteria *root_criteria = create_criteria(config);
	init_default_style(&root_criteria->style);

	init_empty_style(&config->superstyle);

	init_empty_style(&config->hidden_style);
	config->hidden_style.format = strdup("(%h more)");
	config->hidden_style.spec.format = true;

	config->output = strdup("");
	config->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;

	config->max_visible = 5;
	config->sort_criteria = MAKO_SORT_CRITERIA_TIME;
	config->sort_asc = 0;

	config->button_bindings.left = MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION;
	config->button_bindings.right = MAKO_BUTTON_BINDING_DISMISS;
	config->button_bindings.middle = MAKO_BUTTON_BINDING_NONE;

	config->anchor =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
}

void finish_config(struct mako_config *config) {
	struct mako_criteria *criteria, *tmp;
	wl_list_for_each_safe(criteria, tmp, &config->criteria, link) {
		destroy_criteria(criteria);
	}

	finish_style(&config->superstyle);
	finish_style(&config->hidden_style);
	free(config->output);
}

void init_default_style(struct mako_style *style) {
	style->width = 300;
	style->height = 100;

	style->margin.top = 10;
	style->margin.right = 10;
	style->margin.bottom = 10;
	style->margin.left = 10;

	style->padding.top = 5;
	style->padding.right = 5;
	style->padding.bottom = 5;
	style->padding.left = 5;

	style->border_size = 2;

	style->font = strdup("monospace 10");
	style->markup = true;
	style->format = strdup("<b>%s</b>\n%b");

	style->actions = true;
	style->default_timeout = 0;
	style->ignore_timeout = false;

	style->colors.background = 0x285577FF;
	style->colors.text = 0xFFFFFFFF;
	style->colors.border = 0x4C7899FF;

	style->group_criteria_spec.none = true;

	// Everything in the default config is explicitly specified.
	memset(&style->spec, true, sizeof(struct mako_style_spec));
}

void init_empty_style(struct mako_style *style) {
	memset(style, 0, sizeof(struct mako_style));
}

void finish_style(struct mako_style *style) {
	free(style->font);
	free(style->format);
}

// Update `target` with the values specified in `style`. If a failure occurs,
// `target` will remain unchanged.
bool apply_style(struct mako_style *target, const struct mako_style *style) {
	// Try to duplicate strings up front in case allocation fails and we have
	// to bail without changing `target`.
	char *new_font = NULL;
	char *new_format = NULL;

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

	// Now on to actually setting things!

	if (style->spec.width) {
		target->width = style->width;
		target->spec.width = true;
	}

	if (style->spec.height) {
		target->height = style->height;
		target->spec.height = true;
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

	if (style->spec.group_criteria_spec) {
		target->group_criteria_spec = style->group_criteria_spec;
		target->spec.group_criteria_spec = true;
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
	target->spec.margin = true;
	target->spec.padding = true;
	target->spec.border_size = true;
	target->spec.default_timeout = true;
	target->spec.markup = true;
	target->spec.actions = true;
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
		target->border_size = max(style->border_size, target->border_size);
		target->default_timeout =
			max(style->default_timeout, target->default_timeout);

		target->markup |= style->markup;
		target->actions |= style->actions;

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
				}

				++format_pos; // Enough to move to the next match.
				target_format_pos += 2; // This needs to go to the next slot.
			}
		}
	}

	return true;
}

static bool apply_config_option(struct mako_config *config, const char *name,
		const char *value) {
	if (strcmp(name, "max-visible") == 0) {
		return parse_int(value, &config->max_visible);
	} else if (strcmp(name, "output") == 0) {
		free(config->output);
		config->output = strdup(value);
		return true;
	} else if (strcmp(name, "layer") == 0) {
		if (strcmp(value, "background") == 0) {
			config->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
		} else if (strcmp(value, "bottom") == 0) {
			config->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
		} else if (strcmp(value, "top") == 0) {
			config->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		} else if (strcmp(value, "overlay") == 0) {
			config->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
		} else {
			return false;
		}
		return true;
	} else if (strcmp(name, "sort") == 0) {
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
	} else if (strcmp(name, "anchor") == 0) {
		if (strcmp(value, "top-right") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		} else if (strcmp(value, "bottom-right") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		} else if (strcmp(value, "bottom-center") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
		} else if (strcmp(value, "bottom-left") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		} else if (strcmp(value, "top-left") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		} else if (strcmp(value, "top-center") == 0) {
			config->anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
		} else {
			return false;
		}
		return true;
	}

	return false;
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
		return spec->width = parse_int(value, &style->width);
	} else if (strcmp(name, "height") == 0) {
		return spec->height = parse_int(value, &style->height);
	} else if (strcmp(name, "margin") == 0) {
		return spec->margin = parse_directional(value, &style->margin);
	} else if (strcmp(name, "padding") == 0) {
		return spec->padding = parse_directional(value, &style->padding);
	} else if (strcmp(name, "border-size") == 0) {
		return spec->border_size = parse_int(value, &style->border_size);
	} else if (strcmp(name, "border-color") == 0) {
		return spec->colors.border = parse_color(value, &style->colors.border);
	} else if (strcmp(name, "markup") == 0) {
		return spec->markup = parse_boolean(value, &style->markup);
	} else if (strcmp(name, "actions") == 0) {
		return spec->actions = parse_boolean(value, &style->actions);
	} else if (strcmp(name, "format") == 0) {
		free(style->format);
		return spec->format = parse_format(value, &style->format);
	} else if (strcmp(name, "default-timeout") == 0) {
		return spec->default_timeout =
			parse_int(value, &style->default_timeout);
	} else if (strcmp(name, "ignore-timeout") == 0) {
		return spec->ignore_timeout =
			parse_boolean(value, &style->ignore_timeout);
	} else if (strcmp(name, "group") == 0) {
		return spec->group_criteria_spec =
			parse_criteria_spec(value, &style->group_criteria_spec);
	}

	return false;
}

static bool file_exists(const char *path) {
	return path && access(path, R_OK) != -1;
}

static char *get_config_path(void) {
	static const char *config_paths[] = {
		"$HOME/.mako/config",
		"$XDG_CONFIG_HOME/mako/config",
	};

	if (!getenv("XDG_CONFIG_HOME")) {
		char *home = getenv("HOME");
		if (!home) {
			return NULL;
		}
		char config_home[strlen(home) + strlen("/.config") + 1];
		strcpy(config_home, home);
		strcat(config_home, "/.config");
		setenv("XDG_CONFIG_HOME", config_home, 1);
	}

	for (size_t i = 0; i < sizeof(config_paths) / sizeof(char *); ++i) {
		wordexp_t p;
		if (wordexp(config_paths[i], &p, 0) == 0) {
			char *path = strdup(p.we_wordv[0]);
			wordfree(&p);
			if (file_exists(path)) {
				return path;
			}
			free(path);
		}
	}

	return NULL;
}

int load_config_file(struct mako_config *config) {
	char *path = get_config_path();
	if (!path) {
		return 0;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading", path);
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
			free(section);
			section = strndup(line + 1, strlen(line) - 2);
			if (strcmp(section, "hidden") == 0) {
				// Skip making a criteria for the hidden section.
				criteria = NULL;
				continue;
			}
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

		struct mako_style *target_style;
		if (section != NULL && strcmp(section, "hidden") == 0) {
			// The hidden criteria is a lie, we store the associated style
			// directly on the config because there's no "real" notification
			// object to match against it later.
			target_style = &config->hidden_style;
		} else {
			assert(criteria != NULL);
			target_style = &criteria->style;
		}

		valid_option = apply_style_option(target_style, line, eq + 1);

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

	free(section);
	free(line);
	fclose(f);
	free(path);
	return ret;
}

int parse_config_arguments(struct mako_config *config, int argc, char **argv) {
	static const struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"font", required_argument, 0, 0},
		{"background-color", required_argument, 0, 0},
		{"text-color", required_argument, 0, 0},
		{"width", required_argument, 0, 0},
		{"height", required_argument, 0, 0},
		{"margin", required_argument, 0, 0},
		{"padding", required_argument, 0, 0},
		{"border-size", required_argument, 0, 0},
		{"border-color", required_argument, 0, 0},
		{"markup", required_argument, 0, 0},
		{"actions", required_argument, 0, 0},
		{"format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{"default-timeout", required_argument, 0, 0},
		{"ignore-timeout", required_argument, 0, 0},
		{"output", required_argument, 0, 0},
		{"layer", required_argument, 0, 0},
		{"anchor", required_argument, 0, 0},
		{"sort", required_argument, 0, 0},
		{0},
	};

	struct mako_criteria *root_criteria =
		wl_container_of(config->criteria.next, root_criteria, link);

	optind = 1;
	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c < 0) {
			break;
		} else if (c == 'h') {
			return 1;
		} else if (c != 0) {
			return -1;
		}

		const char *name = long_options[option_index].name;
		if (!apply_style_option(&root_criteria->style, name, optarg)
				&& !apply_config_option(config, name, optarg)) {
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

	if (load_config_file(&new_config) != 0) {
		fprintf(stderr, "Failed to reload config\n");
		finish_config(&new_config);
		return -1;
	}

	int ret = parse_config_arguments(&new_config, argc, argv);
	if (ret != 0) {
		return ret;
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
