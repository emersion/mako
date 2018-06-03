#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "config.h"

void init_config(struct mako_config *config) {
	init_default_style(&config->default_style);

	config->hidden_format = strdup("(%h more)");
	config->output = strdup("");
	config->max_visible = 5;

	config->sort_criteria = MAKO_SORT_CRITERIA_TIME;
	config->sort_asc = 0;

	config->button_bindings.left = MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION;
	config->button_bindings.right = MAKO_BUTTON_BINDING_DISMISS;
	config->button_bindings.middle = MAKO_BUTTON_BINDING_NONE;
}

void finish_config(struct mako_config *config) {
	finish_style(&config->default_style);
	free(config->hidden_format);
	free(config->output);
}

void init_default_style(struct mako_style *style) {
	style->width = 300;
	style->height = 100;

	style->margin.top = 10;
	style->margin.right = 10;
	style->margin.bottom = 10;
	style->margin.left = 10;

	style->padding = 5;
	style->border_size = 2;

	style->font = strdup("monospace 10");
	style->markup = true;
	style->format = strdup("<b>%s</b>\n%b");

	style->actions = true;
	style->default_timeout = 0;

	style->colors.background = 0x285577FF;
	style->colors.text = 0xFFFFFFFF;
	style->colors.border = 0x4C7899FF;

	// Everything in the default config is explicitly specified.
	style->spec = (struct mako_style_spec){
		.width = true,
		.height = true,
		.margin = true,
		.padding = true,
		.border_size = true,
		.font = true,
		.markup = true,
		.format = true,
		.actions = true,
		.default_timeout = true,
		.colors = {
			.background = true,
			.text = true,
			.border = true,
		},
	};
}

void finish_style(struct mako_style *style) {
	free(style->font);
	free(style->format);
}

static bool parse_int(const char *s, int *out) {
	errno = 0;
	char *end;
	*out = (int)strtol(s, &end, 10);
	return errno == 0 && end[0] == '\0';
}

/* Parse between 1 and 4 integers, comma separated, from the provided string.
 * Depending on the number of integers provided, the four fields of the `out`
 * struct will be initialized following the same rules as the CSS "margin"
 * property.
 */
static bool parse_directional(const char *directional_string,
		struct mako_directional *out) {
	char *components = strdup(directional_string);

	int32_t values[] = {0, 0, 0, 0};

	char *saveptr = NULL;
	char *token = strtok_r(components, ",", &saveptr);
	size_t count;
	for (count = 0; count < 4; count++) {
		if (token == NULL) {
			break;
		}

		int32_t number;
		if (!parse_int(token, &number)) {
			// There were no digits, or something else went horribly wrong
			free(components);
			return false;
		}

		values[count] = number;
		token = strtok_r(NULL, ",", &saveptr);
	}

	switch (count) {
	case 1: // All values are the same
		out->top = out->right = out->bottom = out->left = values[0];
		break;
	case 2: // Vertical, horizontal
		out->top = out->bottom = values[0];
		out->right = out->left = values[1];
		break;
	case 3: // Top, horizontal, bottom
		out->top = values[0];
		out->right = out->left = values[1];
		out->bottom = values[2];
		break;
	case 4: // Top, right, bottom, left
		out->top = values[0];
		out->right = values[1];
		out->bottom = values[2];
		out->left = values[3];
		break;
	}

	free(components);
	return true;
}

static bool parse_color(const char *color, uint32_t *out) {
	if (color[0] != '#') {
		return false;
	}
	color++;

	size_t len = strlen(color);
	if (len != 6 && len != 8) {
		return false;
	}

	errno = 0;
	char *end;
	*out = (uint32_t)strtoul(color, &end, 16);
	if (errno != 0 || end[0] != '\0') {
		return false;
	}

	if (len == 6) {
		*out = (*out << 8) | 0xFF;
	}
	return true;
}

static bool apply_config_option(struct mako_config *config, const char *section,
		const char *name, const char *value) {
	// First try to parse this as a global option.
	if (strcmp(name, "max-visible") == 0) {
		return parse_int(value, &config->max_visible);
	} else if (strcmp(name, "output") == 0) {
		free(config->output);
		config->output = strdup(value);
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
		}
		return true;
	} else if (section != NULL) {
		// TODO: criteria support
		if (strcmp(section, "hidden") != 0) {
			fprintf(stderr, "Only the 'hidden' section is currently supported\n");
			return false;
		}

		if (strcmp(name, "format") == 0) {
			free(config->hidden_format);
			config->hidden_format = strdup(value);
			return true;
		} else {
			fprintf(stderr, "Only 'format' is supported in the 'hidden' section\n");
			return false;
		}
	}

	// Now try to match on style options.
	struct mako_style *style = &config->default_style;

	if (strcmp(name, "font") == 0) {
		free(style->font);
		style->font = strdup(value);
		return true;
	} else if (strcmp(name, "background-color") == 0) {
		return parse_color(value, &style->colors.background);
	} else if (strcmp(name, "text-color") == 0) {
		return parse_color(value, &style->colors.text);
	} else if (strcmp(name, "width") == 0) {
		return parse_int(value, &style->width);
	} else if (strcmp(name, "height") == 0) {
		return parse_int(value, &style->height);
	} else if (strcmp(name, "margin") == 0) {
		return parse_directional(value, &style->margin);
	} else if (strcmp(name, "padding") == 0) {
		return parse_int(value, &style->padding);
	} else if (strcmp(name, "border-size") == 0) {
		return parse_int(value, &style->border_size);
	} else if (strcmp(name, "border-color") == 0) {
		return parse_color(value, &style->colors.border);
	} else if (strcmp(name, "markup") == 0) {
		style->markup = strcmp(value, "1") == 0;
		return style->markup || strcmp(value, "0") == 0;
	} else if (strcmp(name, "format") == 0) {
		free(style->format);
		style->format = strdup(value);
		return true;
	} else if (strcmp(name, "default-timeout") == 0) {
		return parse_int(value, &style->default_timeout);
	} else {
		return false;
	}
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
			continue;
		}
		char *eq = strchr(line, '=');
		if (!eq) {
			fprintf(stderr, "[%s:%d] Expected key=value\n", base, lineno);
			ret = -1;
			break;
		}
		eq[0] = '\0';
		if (!apply_config_option(config, section, line, eq + 1)) {
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

static int config_argc = 0;
static char **config_argv = NULL;

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
		{"format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{"default-timeout", required_argument, 0, 0},
		{"output", required_argument, 0, 0},
		{"sort", required_argument, 0, 0},
		{0},
	};

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
		if (!apply_config_option(config, NULL, name, optarg)) {
			fprintf(stderr, "Failed to parse option '%s'\n", name);
			return -1;
		}
	}

	config_argc = argc;
	config_argv = argv;

	return 0;
}

void reload_config(struct mako_config *config) {
	finish_config(config);
	init_config(config);
	load_config_file(config);
	parse_config_arguments(config, config_argc, config_argv);
}
