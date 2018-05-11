#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

void init_config(struct mako_config *config) {
	config->font = strdup("monospace 10");

	config->padding = 5;
	config->width = 300;
	config->height = 100;
	config->border_size = 1;
	config->markup = true;
	config->format = strdup("<b>%s</b>\n%b");
	config->actions = true;

	config->margin.top = 10;
	config->margin.right = 10;
	config->margin.bottom = 10;
	config->margin.left = 10;

	config->max_visible = 5;

	config->colors.background = 0x285577FF;
	config->colors.text = 0xFFFFFFFF;
	config->colors.border = 0x4C7899FF;
	config->button_bindings.left = MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION;
	config->button_bindings.right = MAKO_BUTTON_BINDING_DISMISS;
}

void finish_config(struct mako_config *config) {
	free(config->font);
	free(config->format);
}

/* Parse between 1 and 4 integers, comma separated, from the provided string.
 * Depending on the number of integers provided, the four fields of the `out`
 * struct will be initialized following the same rules as the CSS "margin"
 * property. Returns 0 on success, -1 on parsing failure. */
int parse_directional(const char *directional_string,
		struct mako_directional *out) {
	int error = 0;
	char *components = strdup(directional_string);

	char *token, *saveptr = NULL;
	int32_t values[] = {0, 0, 0, 0};
	size_t count;

	token = strtok_r(components, ",", &saveptr);
	for (count = 0; count < 4; count++) {
		if (token == NULL) {
			break;
		}

		errno = 0;

		char *endptr = NULL;
		int32_t number = strtol(token, &endptr, 10);
		if (errno || endptr == token) {
			// There were no digits, or something else went horribly wrong.
			error = -1;
			break;
		}

		values[count] = number;
		token = strtok_r(NULL, ",", &saveptr);
	}

	if (error == 0) {
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
	}

	free(components);
	return error;
}

static uint32_t parse_color(const char *color) {
	if (color[0] != '#') {
		return -1;
	}
	color++;

	size_t len = strlen(color);
	if (len != 6 && len != 8) {
		return -1;
	}
	uint32_t res = (uint32_t)strtoul(color, NULL, 16);
	if (len == 6) {
		res = (res << 8) | 0xFF;
	}
	return res;
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
		{"format", required_argument, 0, 0},
		{"max-visible", required_argument, 0, 0},
		{0},
	};

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
		if (strcmp(name, "font") == 0) {
			free(config->font);
			config->font = strdup(optarg);
		} else if (strcmp(name, "background-color") == 0) {
			config->colors.background = parse_color(optarg);
		} else if (strcmp(name, "text-color") == 0) {
			config->colors.text = parse_color(optarg);
		} else if (strcmp(name, "width") == 0) {
			config->width = strtol(optarg, NULL, 10);
		} else if (strcmp(name, "height") == 0) {
			config->height = strtol(optarg, NULL, 10);
		} else if (strcmp(name, "margin") == 0) {
			if (parse_directional(optarg, &config->margin)) {
				fprintf(stderr, "Unable to parse margins\n");
				return 1;
			}
		} else if (strcmp(name, "padding") == 0) {
			config->padding = strtol(optarg, NULL, 10);
		} else if (strcmp(name, "border-size") == 0) {
			config->border_size = strtol(optarg, NULL, 10);
		} else if (strcmp(name, "border-color") == 0) {
			config->colors.border = parse_color(optarg);
		} else if (strcmp(name, "markup") == 0) {
			config->markup = strcmp(optarg, "1") == 0;
		} else if (strcmp(name, "format") == 0) {
			free(config->format);
			config->format = strdup(optarg);
		} else if (strcmp(name, "max-visible") == 0) {
			config->max_visible = strtol(optarg, NULL, 10);
		}
	}

	return 0;
}
