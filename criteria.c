#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>

#include "enum.h"
#include "mako.h"
#include "config.h"
#include "criteria.h"
#include "mode.h"
#include "notification.h"
#include "surface.h"
#include "wayland.h"

struct mako_criteria *create_criteria(struct mako_config *config) {
	struct mako_criteria *criteria = calloc(1, sizeof(struct mako_criteria));
	if (criteria == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}

	wl_list_insert(config->criteria.prev, &criteria->link);
	return criteria;
}

void free_cond(struct mako_condition *cond) {
	switch (cond->operator) {
	case OP_EQUALS:
	case OP_NOT_EQUALS:
		free(cond->value);
		return;
	case OP_REGEX_MATCHES:
		regfree(&cond->pattern);
		return;
	case OP_NONE:
	case OP_TRUTHY:
	case OP_FALSEY:
		// Nothing to free.
		return;
	}
}

void destroy_criteria(struct mako_criteria *criteria) {
	wl_list_remove(&criteria->link);

	finish_style(&criteria->style);
	free_cond(&criteria->app_name);
	free_cond(&criteria->app_icon);
	free_cond(&criteria->category);
	free_cond(&criteria->desktop_entry);
	free_cond(&criteria->summary);
	free_cond(&criteria->body);
	free(criteria->raw_string);
	free(criteria->output);
	free(criteria->mode);
	free(criteria);
}

static bool match_regex_criteria(regex_t *pattern, char *value) {
	int ret = regexec(pattern, value, 0, NULL, 0);
	if (ret != 0) {
		if (ret != REG_NOMATCH) {
			size_t errlen = regerror(ret, pattern, NULL, 0);
			char errbuf[errlen];
			regerror(ret, pattern, errbuf, sizeof(errbuf));
			fprintf(stderr, "failed to match regex: %s\n", errbuf);
		}
		return false;
	}
	return true;
}

bool match_condition(struct mako_condition *cond, char *value) {
	switch(cond->operator) {
	case OP_EQUALS:
		return strcmp(cond->value, value) == 0;
	case OP_NOT_EQUALS:
		return strcmp(cond->value, value) != 0;
	case OP_REGEX_MATCHES:
		return match_regex_criteria(&cond->pattern, value);
	case OP_TRUTHY:
		return strcmp("", value) != 0;
	case OP_FALSEY:
		return strcmp("", value) == 0;
	case OP_NONE:
		return true;
	}
	abort();
}

bool match_criteria(struct mako_criteria *criteria,
		struct mako_notification *notif) {
	struct mako_criteria_spec spec = criteria->spec;

	if (spec.none) {
		// `none` short-circuits all other criteria.
		return false;
	}

	if (spec.hidden &&
			criteria->hidden != notif->hidden) {
		return false;
	}

	if (spec.app_name &&
			!match_condition(&criteria->app_name, notif->app_name)) {
		return false;
	}

	if (spec.app_icon &&
			!match_condition(&criteria->app_icon, notif->app_icon)) {
		return false;
	}

	if (spec.actionable &&
			criteria->actionable == wl_list_empty(&notif->actions)) {
		return false;
	}

	if (spec.expiring &&
			criteria->expiring != (notif->requested_timeout != 0)) {
		return false;
	}

	if (spec.urgency &&
			criteria->urgency != notif->urgency) {
		return false;
	}

	if (spec.category &&
			!match_condition(&criteria->category, notif->category)) {
		return false;
	}

	if (spec.desktop_entry &&
			!match_condition(&criteria->desktop_entry, notif->desktop_entry)) {
		return false;
	}

	if (spec.summary &&
			!match_condition(&criteria->summary, notif->summary)) {
		return false;
	}

	if (spec.body &&
			!match_condition(&criteria->body, notif->body)) {
		return false;
	}

	if (spec.group_index &&
			criteria->group_index != notif->group_index) {
		return false;
	}

	if (spec.grouped &&
			criteria->grouped != (notif->group_index >= 0)) {
		return false;
	}

	if (spec.anchor && (notif->surface == NULL ||
			criteria->anchor != notif->surface->anchor)) {
		return false;
	}

	if (spec.output && (notif->surface == NULL ||
				notif->surface->surface_output == NULL ||
				strcmp(criteria->output, notif->surface->surface_output->name) != 0)) {
		return false;
	}

	if (spec.mode && !has_mode(notif->state, criteria->mode)) {
		return false;
	}

	return true;
}

bool parse_criteria(const char *string, struct mako_criteria *criteria) {
	// Create space to build up the current token that we're reading. We know
	// that no single token can ever exceed the length of the entire criteria
	// string, so that's a safe length to use for the buffer.
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

		case MAKO_PARSE_STATE_FORMAT:
			// Unsupported state for this parser.
			abort();
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

	// All user-specified criteria are implicitly unhidden by default. This
	// prevents any criteria sections that don't explicitly set `hidden` from
	// styling the hidden pseudo-notification.
	if (!criteria->spec.hidden) {
		criteria->hidden = false;
		criteria->spec.hidden = true;
	}

	criteria->raw_string = strdup(string);
	return true;
}

bool assign_condition(struct mako_condition *cond, enum operator op, char *value) {
	cond->operator = op;
	switch (op) {
		case OP_REGEX_MATCHES:
			if (regcomp(&cond->pattern, value, REG_EXTENDED | REG_NOSUB)) {
				fprintf(stderr, "Invalid regex '%s'\n", value);
				return false;
			}
			return true;
		case OP_EQUALS:
		case OP_NOT_EQUALS:
			cond->value = strdup(value);
			// fall-thru
		case OP_FALSEY:
		case OP_TRUTHY:
		case OP_NONE:
		default:
			return true;
	}
	return true;
}

// Takes a token from the criteria string that looks like
// "key=value", "key!=value", or "key~=value"; and figures
// out which field of the criteria "key" refers to, and sets it to the condition.
// Any further equal signs are assumed to be part of the value. If there is no .
// equal sign present, the field is treated as a boolean, with a leading
// exclamation point signifying negation.
//
// Note that the token will be consumed.
bool apply_criteria_field(struct mako_criteria *criteria, char *token) {
	enum operator op = OP_EQUALS;
	char *key = token;
	char *value = strstr(key, "=");
	bool bare_key = !value;

	if (*key == '\0') {
		return true;
	}

	if (value) {
		if(value[-1] == '~') {
			op = OP_REGEX_MATCHES;
			// shorten the key.
			value[-1] = '\0';
		} else if (value[-1] == '!') {
			op = OP_NOT_EQUALS;
			// shorten the key.
			value[-1] = '\0';
		}
		// Skip past the equal sign to the value itself.
		*value = '\0';
		++value;
	} else {
		// If there's no value, assume it's a boolean, and set the value
		// appropriately. This allows uniform parsing later on.
		if (*key == '!') {
			// Negated boolean, skip past the exclamation point.
			++key;
			op = OP_FALSEY;
			value = "false";
		} else {
			op = OP_TRUTHY;
			value = "true";
		}
	}

	// Now apply the value to the appropriate member of the criteria.
	// If the value was omitted, only try to match against boolean fields.
	// Otherwise, anything is fair game. This helps to return a better error
	// message.

	// String fields can have bare_key, or not bare_key

	if (strcmp(key, "app-name") == 0) {
		criteria->spec.app_name = true;
		return assign_condition(&criteria->app_name, op, value);
	} else if (strcmp(key, "app-icon") == 0) {
		criteria->spec.app_icon = true;
		return assign_condition(&criteria->app_icon, op, value);
	} else if (strcmp(key, "category") == 0) {
		criteria->spec.category = true;
		return assign_condition(&criteria->category, op, value);
	} else if (strcmp(key, "desktop-entry") == 0) {
		criteria->spec.desktop_entry = true;
		return assign_condition(&criteria->desktop_entry, op, value);
	} else if (strcmp(key, "summary") == 0) {
		criteria->spec.summary = true;
		return assign_condition(&criteria->summary, op, value);
	} else if (strcmp(key, "body") == 0) {
		criteria->spec.body = true;
		return assign_condition(&criteria->body, op, value);
	} else if (!bare_key) {
		if (strcmp(key, "urgency") == 0) {
			if (!parse_urgency(value, &criteria->urgency)) {
				fprintf(stderr, "Invalid urgency value '%s'", value);
				return false;
			}
			criteria->spec.urgency = true;
			return true;
		} else if (strcmp(key, "group-index") == 0) {
			if (!parse_int(value, &criteria->group_index)) {
				fprintf(stderr, "Invalid group-index value '%s'", value);
				return false;
			}
			criteria->spec.group_index = true;
			return true;
		} else if (strcmp(key, "anchor") == 0) {
			return criteria->spec.anchor =
				parse_anchor(value, &criteria->anchor);
		} else if (strcmp(key, "output") == 0) {
			criteria->output = strdup(value);
			criteria->spec.output = true;
			return true;
		} else if (strcmp(key, "mode") == 0) {
			criteria->mode = strdup(value);
			criteria->spec.mode = true;
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
	} else if (strcmp(key, "expiring") == 0){
		if (!parse_boolean(value, &criteria->expiring)) {
			fprintf(stderr, "Invalid value '%s' for boolean field '%s'\n",
					value, key);
			return false;
		}
		criteria->spec.expiring = true;
		return true;
	} else if (strcmp(key, "grouped") == 0) {
		if (!parse_boolean(value, &criteria->grouped)) {
			fprintf(stderr, "Invalid value '%s' for boolean field '%s'\n",
					value, key);
			return false;
		}
		criteria->spec.grouped = true;
		return true;
	} else if (strcmp(key, "hidden") == 0) {
		if (!parse_boolean(value, &criteria->hidden)) {
			fprintf(stderr, "Invalid value '%s' for boolean field '%s'\n",
					value, key);
			return false;
		}
		criteria->spec.hidden = true;
		return true;
	} else {
		if (bare_key) {
			fprintf(stderr, "Invalid boolean criteria field '%s'\n", key);
		} else {
			fprintf(stderr, "Invalid criteria field '%s'\n", key);
		}
		return false;
	}

	assert(false && "Criteria parser fell through");
}

// Retrieve the global criteria from a given mako_config. This just so happens
// to be the first criteria in the list.
struct mako_criteria *global_criteria(struct mako_config *config) {
	struct mako_criteria *criteria =
		wl_container_of(config->criteria.next, criteria, link);
	return criteria;
}

// Iterate through `criteria_list`, applying the style from each matching
// criteria to `notif`. Returns the number of criteria that matched, or -1 if
// a failure occurs.
ssize_t apply_each_criteria(struct wl_list *criteria_list,
		struct mako_notification *notif) {
	ssize_t match_count = 0;

	struct mako_criteria *criteria;
	wl_list_for_each(criteria, criteria_list, link) {
		if (!match_criteria(criteria, notif)) {
			continue;
		}
		++match_count;

		if (!apply_style(&notif->style, &criteria->style)) {
			return -1;
		}
	}

	struct mako_surface *surface;
	wl_list_for_each(surface, &notif->state->surfaces, link) {
		if (!strcmp(surface->configured_output, notif->style.output) &&
				surface->anchor == notif->style.anchor &&
				surface->layer == notif->style.layer) {
			notif->surface = surface;
			break;
		}
	}

	if (!notif->surface) {
		notif->surface = create_surface(notif->state, notif->style.output,
			notif->style.layer, notif->style.anchor);
	}

	return match_count;
}

// Given a notification and a criteria spec, create a criteria that matches the
// specified fields of that notification. Unlike create_criteria, this new
// criteria will not be automatically inserted into the configuration. It is
// instead intended to be used for comparing notifications. The spec will be
// copied, so the caller is responsible for doing whatever it needs to do with
// the original after the call completes.
struct mako_criteria *create_criteria_from_notification(
		struct mako_notification *notif, struct mako_criteria_spec *spec) {
	struct mako_criteria *criteria = calloc(1, sizeof(struct mako_criteria));
	if (criteria == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}

	wl_list_init(&criteria->link);

	memcpy(&criteria->spec, spec, sizeof(struct mako_criteria_spec));

	// We only really need to copy the ones that are in the spec, but it
	// doesn't hurt anything to do the rest and it makes this code much nicer
	// to look at.
	criteria->app_name.operator = OP_EQUALS;
	criteria->app_name.value = strdup(notif->app_name);
	criteria->app_icon.operator = OP_EQUALS;
	criteria->app_icon.value = strdup(notif->app_icon);
	criteria->actionable = !wl_list_empty(&notif->actions);
	criteria->expiring = (notif->requested_timeout != 0);
	criteria->urgency = notif->urgency;
	criteria->category.operator = OP_EQUALS;
	criteria->category.value = strdup(notif->category);
	criteria->desktop_entry.operator = OP_EQUALS;
	criteria->desktop_entry.value = strdup(notif->desktop_entry);
	criteria->summary.operator = OP_EQUALS;
	criteria->summary.value = strdup(notif->summary);
	criteria->body.operator = OP_EQUALS;
	criteria->body.value = strdup(notif->body);
	criteria->group_index = notif->group_index;
	criteria->grouped = (notif->group_index >= 0);
	criteria->hidden = notif->hidden;

	return criteria;
}


// To keep the behavior of criteria predictable, there are a few rules that we
// have to impose on what can be modified depending on what was matched.
bool validate_criteria(struct mako_criteria *criteria) {
	char * invalid_option = NULL;

	if (criteria->spec.grouped ||
			criteria->spec.group_index ||
			criteria->spec.output ||
			criteria->spec.anchor) {
		if (criteria->style.spec.anchor) {
			invalid_option = "anchor";
		} else if (criteria->style.spec.output) {
			invalid_option = "output";
		} else if (criteria->style.spec.group_criteria_spec) {
			invalid_option = "group-by";
		}

		if (invalid_option) {
			fprintf(stderr,
					"Setting `%s` is not allowed when matching `grouped`, "
					"`group-index`, `output`, or `anchor`\n",
					invalid_option);
			return false;
		}
	}

	struct mako_criteria_spec copy = {0};
	memcpy(&copy, &criteria->spec, sizeof(struct mako_criteria_spec));
	copy.output = false;
	copy.anchor = false;
	copy.hidden = false;
	bool any_but_surface = mako_criteria_spec_any(&copy);

	if (criteria->style.max_visible && any_but_surface) {
		fprintf(stderr, "Setting `max_visible` is allowed only for `output` "
				"and/or `anchor`\n");
		return false;
	}

	// Hidden is almost always specified, need to look at the actual value.
	if (criteria->hidden && any_but_surface) {
		fprintf(stderr, "Can only set `hidden` along with `output` "
				"and/or `anchor`\n");
		return false;
	}

	if (criteria->style.spec.group_criteria_spec) {
		struct mako_criteria_spec *spec = &criteria->style.group_criteria_spec;

		if (spec->group_index) {
			invalid_option = "group-index";
		} else if (spec->grouped) {
			invalid_option = "grouped";
		} else if (spec->anchor) {
			invalid_option = "anchor";
		} else if (spec->output) {
			invalid_option = "output";
		}

		if (invalid_option) {
			fprintf(stderr, "`%s` cannot be used in `group-by`\n",
					invalid_option);
			return false;
		}
	}

	return true;
}
