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
	config->font = strdup("monospace 10");

	config->padding = 5;
	config->width = 300;
	config->height = 100;
	config->border_size = 2;
	config->markup = true;
	config->format = strdup("<b>%s</b>\n%b");
	config->hidden_format = strdup("%t[%h]");
	config->actions = true;

	config->margin.top = 10;
	config->margin.right = 10;
	config->margin.bottom = 10;
	config->margin.left = 10;

	config->max_visible = 5;
	config->output = strdup("");

	config->colors.background = 0x285577FF;
	config->colors.text = 0xFFFFFFFF;
	config->colors.border = 0x4C7899FF;
	config->button_bindings.left = MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION;
	config->button_bindings.right = MAKO_BUTTON_BINDING_DISMISS;
	config->button_bindings.middle = MAKO_BUTTON_BINDING_NONE;
}

void finish_config(struct mako_config *config) {
	free(config->font);
	free(config->format);
	free(config->hidden_format);
	free(config->output);
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

static bool apply_config_option(struct mako_config *config,
		const char *name, const char *value) {
	if (strcmp(name, "font") == 0) {
		free(config->font);
		config->font = strdup(value);
		return true;
	} else if (strcmp(name, "background-color") == 0) {
		return parse_color(value, &config->colors.background);
	} else if (strcmp(name, "text-color") == 0) {
		return parse_color(value, &config->colors.text);
	} else if (strcmp(name, "width") == 0) {
		return parse_int(value, &config->width);
	} else if (strcmp(name, "height") == 0) {
		return parse_int(value, &config->height);
	} else if (strcmp(name, "margin") == 0) {
		return parse_directional(value, &config->margin);
	} else if (strcmp(name, "padding") == 0) {
		return parse_int(value, &config->padding);
	} else if (strcmp(name, "border-size") == 0) {
		return parse_int(value, &config->border_size);
	} else if (strcmp(name, "border-color") == 0) {
		return parse_color(value, &config->colors.border);
	} else if (strcmp(name, "markup") == 0) {
		config->markup = strcmp(value, "1") == 0;
		return config->markup || strcmp(value, "0") == 0;
	} else if (strcmp(name, "format") == 0) {
		free(config->format);
		config->format = strdup(value);
		return true;
	} else if (strcmp(name, "hidden-format") == 0) {
		free(config->hidden_format);
		config->hidden_format = strdup(value);
		return true;
	} else if (strcmp(name, "max-visible") == 0) {
		return parse_int(value, &config->max_visible);
	} else if (strcmp(name, "default-timeout") == 0) {
		return parse_int(value, &config->default_timeout);
	} else if (strcmp(name, "output") == 0) {
		free(config->output);
		config->output = strdup(value);
		return true;
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
		char *config_home = malloc(strlen(home) + strlen("/.config") + 1);
		if (!config_home) {
			fprintf(stderr, "Unable to allocate $HOME/.config\n");
		} else {
			strcpy(config_home, home);
			strcat(config_home, "/.config");
			setenv("XDG_CONFIG_HOME", config_home, 1);
			free(config_home);
		}
	}

	wordexp_t p;
	char *path;

	for (size_t i = 0; i < sizeof(config_paths) / sizeof(char *); ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			path = strdup(p.we_wordv[0]);
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
		return 1;
	}
	const char *base = basename(path);

	int lineno = 0;
	char *line = NULL;
	size_t n = 0;
	while (getline(&line, &n, f) > 0) {
		++lineno;
		if (line[0] == 0 || line[0] == '\n' || line[0] == '#') {
			continue;
		}
		char *eq = strchr(line, '=');
		if (!eq) {
			fprintf(stderr, "[%s:%d] Expected key=value\n",
					base, lineno);
			goto error;
		}
		*eq = 0;
		if (eq[strlen(eq + 1)] == '\n') {
			eq[strlen(eq + 1)] = 0;
		}
		if (!apply_config_option(config, line, eq + 1)) {
			fprintf(stderr, "[%s:%d] Failed to parse option '%s'\n",
				base, lineno, line);
			goto error;
		}
	}
	free(line);
	fclose(f);
	free(path);
	return 0;
error:
	free(line);
	fclose(f);
	free(path);
	return 1;
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
		{"hidden-format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{"default-timeout", required_argument, 0, 0},
		{"output", required_argument, 0, 0},
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
		if (!apply_config_option(config, name, optarg)) {
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
