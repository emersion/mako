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


const char VALID_FORMAT_SPECIFIERS[] = "%asbhtgi";


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
	int parsed;
	parsed = (int)strtol(string, &end, 10);
	if (errno == 0 && end[0] == '\0') {
		*out = parsed;
		return true;
	} else {
		return false;
	}
}

bool parse_int_ge(const char *string, int *out, int min) {
	int parsed;
	if (parse_int(string, &parsed) && parsed >= min) {
		*out = parsed;
		return true;
	} else {
		return false;
	}
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

bool parse_mako_color(const char *string, struct mako_color *out) {
	char *components = strdup(string);

	char *saveptr = NULL;
	char *token = strtok_r(components, " \t", &saveptr);

	if (token[0] == '#') {
		out->operator = CAIRO_OPERATOR_OVER;
	} else {
		if (strcasecmp(token, "over") == 0) {
			out->operator = CAIRO_OPERATOR_OVER;
		} else if (strcasecmp(token, "source") == 0) {
			out->operator = CAIRO_OPERATOR_SOURCE;
		} else {
			free(components);
			return false;
		}

		token = strtok_r(NULL, " \t", &saveptr);
		if (token == NULL) {
			free(components);
			return false;
		}
	}

	bool ok = parse_color(token, &out->value);

	free(components);
	return ok;
}

bool parse_urgency(const char *string, enum mako_notification_urgency *out) {
	if (strcasecmp(string, "low") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_LOW;
		return true;
	} else if (strcasecmp(string, "normal") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_NORMAL;
		return true;
	} else if (strcasecmp(string, "critical") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_CRITICAL;
		return true;
	} else if (strcasecmp(string, "high") == 0) {
		*out = MAKO_NOTIFICATION_URGENCY_CRITICAL;
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

bool parse_criteria_spec(const char *string, struct mako_criteria_spec *out) {
	// Clear any existing specified fields in the output spec.
	memset(out, 0, sizeof(struct mako_criteria_spec));

	char *components = strdup(string);
	char *saveptr = NULL;
	char *token = strtok_r(components, ",", &saveptr);

	while (token) {
		// Can't just use &= because then we nave no way to report invalid
		// values. :(
		if (strcmp(token, "app-name") == 0) {
			out->app_name = true;
		} else if (strcmp(token, "app-icon") == 0) {
			out->app_icon = true;
		} else if (strcmp(token, "actionable") == 0) {
			out->actionable = true;
		} else if (strcmp(token, "expiring") == 0) {
			out->expiring = true;
		} else if (strcmp(token, "urgency") == 0) {
			out->urgency = true;
		} else if (strcmp(token, "category") == 0) {
			out->category = true;
		} else if (strcmp(token, "desktop-entry") == 0) {
			out->desktop_entry = true;
		} else if (strcmp(token, "summary") == 0) {
			out->summary = true;
		} else if (strcmp(token, "body") == 0) {
			out->body = true;
		} else if (strcmp(token, "grouped") == 0) {
			out->grouped = true;
		} else if (strcmp(token, "group-index") == 0) {
			out->group_index = true;
		} else if (strcmp(token, "anchor") == 0) {
			out->anchor = true;
		} else if (strcmp(token, "output") == 0) {
			out->output = true;
		} else if (strcmp(token, "none") == 0) {
			out->none = true;
		} else {
			fprintf(stderr, "Unknown criteria field '%s'\n", token);
			free(components);
			return false;
		}

		token = strtok_r(NULL, ",", &saveptr);
	}

	free(components);
	return true;
}

// Checks whether any of the fields of the given specification are set. Useful
// for checking for some subset of fields without enumerating all known fields
// yourself. Often you will want to copy a spec and clear fields you _don't_
// care about to use this.
bool mako_criteria_spec_any(const struct mako_criteria_spec *spec) {
	return
		spec->app_name ||
		spec->app_icon ||
		spec->actionable ||
		spec->expiring ||
		spec->urgency ||
		spec->category ||
		spec->desktop_entry ||
		spec->summary ||
		spec->body ||
		spec->none ||
		spec->group_index ||
		spec->grouped ||
		spec->hidden ||
		spec->output ||
		spec->anchor;
}

bool parse_format(const char *string, char **out) {
	size_t token_max_length = strlen(string) + 1;
	char token[token_max_length];
	memset(token, 0, token_max_length);
	size_t token_location = 0;

	enum mako_parse_state state = MAKO_PARSE_STATE_NORMAL;
	for (size_t i = 0; i < token_max_length; ++i) {
		char ch = string[i];

		switch (state) {
		case MAKO_PARSE_STATE_FORMAT:
			if (!strchr(VALID_FORMAT_SPECIFIERS, ch)) {
				// There's an invalid format specifier, bail.
				*out = NULL;
				return false;
			}

			token[token_location] = ch;
			++token_location;
			state = MAKO_PARSE_STATE_NORMAL;
			break;

		case MAKO_PARSE_STATE_ESCAPE:
			switch (ch) {
			case 'n':
				token[token_location] = '\n';
				++token_location;
				break;

			case '\\':
				token[token_location] = '\\';
				++token_location;
				break;

			default:
				++token_location;
				token[token_location] = ch;
				++token_location;
				break;
			}

			state = MAKO_PARSE_STATE_NORMAL;
			break;

		case MAKO_PARSE_STATE_NORMAL:
			switch (ch) {
			case '\\':
				token[token_location] = ch;
				state = MAKO_PARSE_STATE_ESCAPE;
				break;

			case '%':
				token[token_location] = ch;
				++token_location; // Leave the % intact.
				state = MAKO_PARSE_STATE_FORMAT;
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

bool parse_anchor(const char *string, uint32_t *out) {
	if (strcmp(string, "top-right") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	} else if (strcmp(string, "top-center") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
	} else if (strcmp(string, "top-left") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	} else if (strcmp(string, "bottom-right") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	} else if (strcmp(string, "bottom-center") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	} else if (strcmp(string, "bottom-left") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	} else if (strcmp(string, "center-right") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	} else if (strcmp(string, "center-left") == 0) {
		*out = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	} else if (strcmp(string, "center") == 0) {
		*out = 0;
	} else {
		fprintf(stderr, "Invalid anchor value '%s'\n", string);
		return false;
	}

	return true;
}
