#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wayland-client.h>
#include <wordexp.h>
#include <unistd.h>

#include "enum.h"
#include "types.h"

bool parse_boolean(const char *string, bool *out) {
	if (strcasecmp(string, "true") == 0 || strcmp(string, "1") == 0) {
		*out = true;
		return true;
	} else if (strcasecmp(string, "false") == 0 || strcmp(string, "0") == 0) {
		*out = false;
		return true;
	}

	return false;
}

bool parse_int(const char *string, int *out) {
	errno = 0;
	char *end;
	*out = (int)strtol(string, &end, 10);
	return errno == 0 && end[0] == '\0';
}

bool parse_color(const char *string, uint32_t *out) {
	if (string[0] != '#') {
		return false;
	}
	string++;

	size_t len = strlen(string);
	if (len != 6 && len != 8) {
		return false;
	}

	errno = 0;
	char *end;
	*out = (uint32_t)strtoul(string, &end, 16);
	if (errno != 0 || end[0] != '\0') {
		return false;
	}

	if (len == 6) {
		*out = (*out << 8) | 0xFF;
	}
	return true;
}

bool parse_urgency(const char *string, enum mako_notification_urgency *out) {
	if (strcasecmp(string, "low") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_LOW;
		return true;
	} else if (strcasecmp(string, "normal") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_NORMAL;
		return true;
	} else if (strcasecmp(string, "high") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_HIGH;
		return true;
	}

	return false;
}

/* Parse between 1 and 4 integers, comma separated, from the provided string.
 * Depending on the number of integers provided, the four fields of the `out`
 * struct will be initialized following the same rules as the CSS "margin"
 * property.
 */
bool parse_directional(const char *string, struct mako_directional *out) {
	char *components = strdup(string);

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

bool parse_format(const char *string, char **out) {
	int token_max_length = strlen(string) + 1;
	char token[token_max_length];
	memset(token, 0, token_max_length);
	size_t token_location = 0;

	enum mako_parse_state state = MAKO_PARSE_STATE_NORMAL;
	const char *location = string;

	char ch;
	while ((ch = *location++) != '\0') {
		switch (state) {
		case MAKO_PARSE_STATE_ESCAPE:
			switch (ch) {
			case 'n':
				token[token_location] = '\n';
				++token_location;
				break;

			case '\\':
			default:
				token[token_location] = ch;
				++token_location;
				break;
			}

			state = MAKO_PARSE_STATE_NORMAL;
			break;

		case MAKO_PARSE_STATE_NORMAL:
			switch (ch) {
			case '\\':
				state = MAKO_PARSE_STATE_ESCAPE;
				break;

			default:
				token[token_location] = ch;
				++token_location;
				break;
			}
			break;

		default:
			*out = NULL;
			return false;
		}
	}

	*out = strdup(token);
	return true;
}
