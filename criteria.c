#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wayland-client.h>
#include "mako.h"
#include "config.h"
#include "criteria.h"
#include "notification.h"

struct mako_criteria *create_criteria(struct mako_config *config) {
	struct mako_criteria *criteria = calloc(1, sizeof(struct mako_criteria));
	if (criteria == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}

	wl_list_insert(config->criteria.prev, &criteria->link);
	return criteria;
}

void destroy_criteria(struct mako_criteria *criteria) {
	wl_list_remove(&criteria->link);

	finish_style(&criteria->style);
	free(criteria->app_name);
	free(criteria->app_icon);
	free(criteria->category);
	free(criteria->desktop_entry);
	free(criteria);
}

bool match_criteria(struct mako_criteria *criteria,
		struct mako_notification *notif) {
	struct mako_criteria_spec spec = criteria->spec;

	if (spec.app_name &&
			strcmp(criteria->app_name, notif->app_name) != 0) {
		return false;
	}

	if (spec.app_icon &&
			strcmp(criteria->app_icon, notif->app_icon) != 0) {
		return false;
	}

	if (spec.actionable &&
			criteria->actionable == wl_list_empty(&notif->actions)) {
		return false;
	}

	if (spec.urgency &&
			criteria->urgency != notif->urgency) {
		return false;
	}

	if (spec.category &&
			strcmp(criteria->category, notif->category) != 0) {
		return false;
	}

	if (spec.desktop_entry &&
			strcmp(criteria->desktop_entry, notif->desktop_entry) != 0) {
		return false;
	}

	return true;
}

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

bool parse_criteria(const char *string, struct mako_criteria *criteria) {
	// Create space to build up the current token that we're reading. We know
	// that no single token can ever exceed the length of the entire criteria
	// string, so that's a safe length to use for the buffer.
	int token_max_length = strlen(string) + 1;
	char token[token_max_length];
	memset(token, 0, token_max_length);
	int token_location = 0;

	enum mako_parse_state state = MAKO_PARSE_STATE_NORMAL;
	const char *location = string;

	char ch;
	while ((ch = *location++) != '\0') {
		switch (state) {
		case MAKO_PARSE_STATE_ESCAPE:
		case MAKO_PARSE_STATE_QUOTE_ESCAPE:
			token[token_location] = ch;
			++token_location;
			state &= ~MAKO_PARSE_STATE_ESCAPE; // These work as a bitmask.
			break;

		case MAKO_PARSE_STATE_QUOTE:
			switch (ch) {
			case '\\':
				state = MAKO_PARSE_STATE_QUOTE_ESCAPE;
				break;
			case '"':
				state = MAKO_PARSE_STATE_NORMAL;
				break;
			case ' ':
			default:
				token[token_location] = ch;
				++token_location;
			}
			break;

		case MAKO_PARSE_STATE_NORMAL:
			switch (ch) {
			case '\\':
				state = MAKO_PARSE_STATE_ESCAPE;
				break;
			case '"':
				state = MAKO_PARSE_STATE_QUOTE;
				break;
			case ' ':
				// New token, apply the old one and reset our state.
				if (!apply_criteria_field(criteria, token)) {
					// An error should have been printed already.
					return false;
				}
				memset(token, 0, token_max_length);
				token_location = 0;
				break;
			default:
				token[token_location] = ch;
				++token_location;
			}
			break;
		}
	}

	if (state != MAKO_PARSE_STATE_NORMAL) {
		if (state & MAKO_PARSE_STATE_QUOTE) {
			fprintf(stderr, "Unmatched quote in criteria definition\n");
			return false;
		} else if (state & MAKO_PARSE_STATE_ESCAPE) {
			fprintf(stderr, "Trailing backslash in criteria definition\n");
			return false;
		} else {
			fprintf(stderr, "Got confused parsing criteria definition\n");
			return false;
		}
	}

	// Apply the last token, which will be left in the buffer after we hit the
	// final NULL. We know it's valid since we just checked for that.
	if (!apply_criteria_field(criteria, token)) {
		// An error should have been printed by this point, we don't need to.
		return false;
	}

	return true;
}

// Takes a token from the criteria string that looks like "key=value", figures
// out which field of the criteria "key" refers to, and sets it to "value".
// Any further equal signs are assumed to be part of the value. If there is no .
// equal sign present, the field is treated as a boolean, with a leading
// exclamation point signifying negation.
//
// Note that the token will be consumed.
bool apply_criteria_field(struct mako_criteria *criteria, char *token) {
	char *key = token;
	char *value = strstr(key, "=");
	bool bare_key = !value;

	if (*key == '\0') {
		return true;
	}

	if (value) {
		// Skip past the equal sign to the value itself.
		*value = '\0';
		++value;
	} else {
		// If there's no value, assume it's a boolean, and set the value
		// appropriately. This allows uniform parsing later on.
		if (*key == '!') {
			// Negated boolean, skip past the exclamation point.
			++key;
			value = "false";
		} else {
			value = "true";
		}
	}

	// Now apply the value to the appropriate member of the criteria.
	// If the value was omitted, only try to match against boolean fields.
	// Otherwise, anything is fair game. This helps to return a better error
	// message.

	if (!bare_key) {
		if (strcmp(key, "app-name") == 0) {
			criteria->app_name = strdup(value);
			criteria->spec.app_name = true;
			return true;
		} else if (strcmp(key, "app-icon") == 0) {
			criteria->app_icon = strdup(value);
			criteria->spec.app_icon = true;
			return true;
		} else if (strcmp(key, "urgency") == 0) {
			if (!parse_urgency(value, &criteria->urgency)) {
				fprintf(stderr, "Invalid urgency value '%s'", value);
				return false;
			}
			criteria->spec.urgency = true;
			return true;
		} else if (strcmp(key, "category") == 0) {
			criteria->category = strdup(value);
			criteria->spec.category = true;
			return true;
		} else if (strcmp(key, "desktop-entry") == 0) {
			criteria->desktop_entry = strdup(value);
			criteria->spec.desktop_entry = true;
			return true;
		} else {
			// Anything left must be one of the boolean fields, defined using
			// standard syntax. Continue on.
		}
	}

	if (strcmp(key, "actionable") == 0) {
		if (!parse_boolean(value, &criteria->actionable)) {
			fprintf(stderr, "Invalid value '%s' for boolean field '%s'\n",
					value, key);
			return false;
		}
		criteria->spec.actionable = true;
		return true;
	} else {
		if (bare_key) {
			fprintf(stderr, "Invalid boolean criteria field '%s'\n", key);
		} else {
			fprintf(stderr, "Invalid criteria field '%s'\n", key);
		}
		return false;
	}

	return true;
}

// Retreive the global critiera from a given mako_config. This just so happens
// to be the first criteria in the list.
struct mako_criteria *global_criteria(struct mako_config *config) {
	struct mako_criteria *criteria =
		wl_container_of(config->criteria.next, criteria, link);
	return criteria;
}


// Iterate through `criteria_list`, applying the style from each matching
// criteria to `notif`. Returns the number of criteria that matched, or -1 if
// a failure occurs.
int apply_each_criteria(struct wl_list *criteria_list,
		struct mako_notification *notif) {
	int match_count = 0;

	struct mako_criteria *criteria;
	wl_list_for_each(criteria, criteria_list, link) {
		if (!match_criteria(criteria, notif)) {
			continue;
		}
		++match_count;

		if (!apply_style(&criteria->style, &notif->style)) {
			return -1;
		}
	}

	return match_count;
}
