#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>
#include <wayland-client.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif

#include "config.h"
#include "criteria.h"
#include "dbus.h"
#include "event-loop.h"
#include "mako.h"
#include "notification.h"

bool hotspot_at(struct mako_hotspot *hotspot, int32_t x, int32_t y) {
	return x >= hotspot->x &&
		y >= hotspot->y &&
		x < hotspot->x + hotspot->width &&
		y < hotspot->y + hotspot->height;
}

void reset_notification(struct mako_notification *notif) {
	struct mako_action *action, *tmp;
	wl_list_for_each_safe(action, tmp, &notif->actions, link) {
		wl_list_remove(&action->link);
		free(action->key);
		free(action->title);
		free(action);
	}

	notif->urgency = MAKO_NOTIFICATION_URGENCY_UNKNOWN;
	notif->progress = -1;

	destroy_timer(notif->timer);
	notif->timer = NULL;

	free(notif->app_name);
	free(notif->app_icon);
	free(notif->summary);
	free(notif->body);
	free(notif->category);
	free(notif->desktop_entry);

	notif->app_name = NULL;
	notif->app_icon = NULL;
	notif->summary = NULL;
	notif->body = NULL;
	notif->category = NULL;
	notif->desktop_entry = NULL;
}

struct mako_notification *create_notification(struct mako_state *state) {
	struct mako_notification *notif =
		calloc(1, sizeof(struct mako_notification));
	if (notif == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}

	notif->state = state;
	++state->last_id;
	notif->id = state->last_id;
	wl_list_init(&notif->actions);
	reset_notification(notif);

	return notif;
}

void destroy_notification(struct mako_notification *notif) {
	wl_list_remove(&notif->link);

	reset_notification(notif);
	finish_style(&notif->style);
	free(notif);
}

void close_notification(struct mako_notification *notif,
		enum mako_notification_close_reason reason) {
	notify_notification_closed(notif, reason);
	wl_list_remove(&notif->link);  // Remove so regrouping works...
	wl_list_init(&notif->link);  // ...but destroy will remove again.

	struct mako_criteria *notif_criteria = create_criteria_from_notification(
			notif, &notif->style.group_criteria_spec);
	if (notif_criteria) {
		group_notifications(notif->state, notif_criteria);
		free(notif_criteria);
	}

	destroy_notification(notif);
}

struct mako_notification *get_notification(struct mako_state *state,
		uint32_t id) {
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->id == id) {
			return notif;
		}
	}
	return NULL;
}

void close_all_notifications(struct mako_state *state,
		enum mako_notification_close_reason reason) {
	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		close_notification(notif, reason);
	}
}

static size_t trim_space(char *dst, const char *src) {
	size_t src_len = strlen(src);
	const char *start = src;
	const char *end = src + src_len;

	while (start != end && isspace(start[0])) {
		++start;
	}

	while (end != start && isspace(end[-1])) {
		--end;
	}

	size_t trimmed_len = end - start;
	memmove(dst, start, trimmed_len);
	dst[trimmed_len] = '\0';
	return trimmed_len;
}

static const char *escape_markup_char(char c) {
	switch (c) {
	case '&': return "&amp;";
	case '<': return "&lt;";
	case '>': return "&gt;";
	case '\'': return "&apos;";
	case '"': return "&quot;";
	}
	return NULL;
}

static size_t escape_markup(const char *s, char *buf) {
	size_t len = 0;
	while (s[0] != '\0') {
		const char *replacement = escape_markup_char(s[0]);
		if (replacement != NULL) {
			size_t replacement_len = strlen(replacement);
			if (buf != NULL) {
				memcpy(buf + len, replacement, replacement_len);
			}
			len += replacement_len;
		} else {
			if (buf != NULL) {
				buf[len] = s[0];
			}
			++len;
		}
		++s;
	}
	if (buf != NULL) {
		buf[len] = '\0';
	}
	return len;
}

static char *mako_asprintf(const char *fmt, ...) {
	char *text;
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (size < 0) {
		return NULL;
	}

	text = malloc(size + 1);
	if (text == NULL) {
		return NULL;
	}

	va_start(args, fmt);
	vsnprintf(text, size + 1, fmt, args);
	va_end(args);

	return text;
}

// Any new format specifiers must also be added to VALID_FORMAT_SPECIFIERS.

char *format_state_text(char variable, bool *markup, void *data) {
	struct mako_state *state = data;
	switch (variable) {
	case 'h':;
		int hidden = wl_list_length(&state->notifications) - state->config.max_visible;
		return mako_asprintf("%d", hidden);
	case 't':;
		int count = wl_list_length(&state->notifications);
		return mako_asprintf("%d", count);
	}
	return NULL;
}

char *format_notif_text(char variable, bool *markup, void *data) {
	struct mako_notification *notif = data;
	switch (variable) {
	case 'a':
		return strdup(notif->app_name);
	case 's':
		return strdup(notif->summary);
	case 'b':
		*markup = notif->style.markup;
		return strdup(notif->body);
	}
	return NULL;
}

size_t format_text(const char *format, char *buf, mako_format_func_t format_func, void *data) {
	size_t len = 0;

	const char *last = format;
	while (1) {
		char *current = strchr(last, '%');
		if (current == NULL || current[1] == '\0') {
			size_t tail_len = strlen(last);
			if (buf != NULL) {
				memcpy(buf + len, last, tail_len + 1);
			}
			len += tail_len;
			break;
		}

		size_t chunk_len = current - last;
		if (buf != NULL) {
			memcpy(buf + len, last, chunk_len);
		}
		len += chunk_len;

		char *value = NULL;
		bool markup = false;

		if (current[1] == '%') {
			value = strdup("%");
		} else {
			value =	format_func(current[1], &markup, data);
		}
		if (value == NULL) {
			value = strdup("");
		}

		size_t value_len;
		if (!markup || !pango_parse_markup(value, -1, 0, NULL, NULL, NULL, NULL)) {
			char *escaped = NULL;
			if (buf != NULL) {
				escaped = buf + len;
			}
			value_len = escape_markup(value, escaped);
		} else {
			value_len = strlen(value);
			if (buf != NULL) {
				memcpy(buf + len, value, value_len);
			}
		}
		free(value);

		len += value_len;
		last = current + 2;
	}

	if (buf != NULL) {
		trim_space(buf, buf);
	}
	return len;
}

static enum mako_button_binding get_button_binding(struct mako_config *config,
		uint32_t button) {
	switch (button) {
	case BTN_LEFT:
		return config->button_bindings.left;
	case BTN_RIGHT:
		return config->button_bindings.right;
	case BTN_MIDDLE:
		return config->button_bindings.middle;
	}
	return MAKO_BUTTON_BINDING_NONE;
}

void notification_handle_button(struct mako_notification *notif, uint32_t button,
		enum wl_pointer_button_state state) {
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}

	switch (get_button_binding(&notif->state->config, button)) {
	case MAKO_BUTTON_BINDING_NONE:
		break;
	case MAKO_BUTTON_BINDING_DISMISS:
		close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BUTTON_BINDING_DISMISS_ALL:
		close_all_notifications(notif->state, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BUTTON_BINDING_INVOKE_DEFAULT_ACTION:;
		struct mako_action *action;
		wl_list_for_each(action, &notif->actions, link) {
			if (strcmp(action->key, DEFAULT_ACTION_KEY) == 0) {
				notify_action_invoked(action);
				break;
			}
		}
		close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	}
}

/*
 * Searches through the notifications list and returns the next position at
 * which to insert. If no results for the specified urgency are found,
 * it will return the closest link searching in the direction specifed.
 * (-1 for lower, 1 or upper).
 */
static struct wl_list *get_last_notif_by_urgency(struct wl_list *notifications,
		enum mako_notification_urgency urgency, int direction) {
	enum mako_notification_urgency current = urgency;

	if (wl_list_empty(notifications)) {
		return notifications;
	}

	while (current <= MAKO_NOTIFICATION_URGENCY_HIGH &&
		current >= MAKO_NOTIFICATION_URGENCY_UNKNOWN) {
		struct mako_notification *notif;
		wl_list_for_each_reverse(notif, notifications, link) {
			if (notif->urgency == current) {
				return &notif->link;
			}
		}
		current += direction;
	}

	return notifications;
}

void insert_notification(struct mako_state *state, struct mako_notification *notif) {
	struct mako_config *config = &state->config;
	struct wl_list *insert_node;

	if (config->sort_criteria == MAKO_SORT_CRITERIA_TIME &&
			!(config->sort_asc & MAKO_SORT_CRITERIA_TIME)) {
		insert_node = &state->notifications;
	} else if (config->sort_criteria == MAKO_SORT_CRITERIA_TIME &&
			(config->sort_asc & MAKO_SORT_CRITERIA_TIME)) {
		insert_node = state->notifications.prev;
	} else if (config->sort_criteria & MAKO_SORT_CRITERIA_URGENCY) {
		int direction = (config->sort_asc & MAKO_SORT_CRITERIA_URGENCY) ? -1 : 1;
		int offset = 0;
		if (!(config->sort_asc & MAKO_SORT_CRITERIA_TIME)) {
			offset = direction;
		}
		insert_node = get_last_notif_by_urgency(&state->notifications,
			notif->urgency + offset, direction);
	} else {
		insert_node = &state->notifications;
	}

	wl_list_insert(insert_node, &notif->link);
}

// Iterate through all of the current notifications and group any that share
// the same values for all of the criteria fields in `spec`. Returns the number
// of notifications in the resulting group, or -1 if something goes wrong
// with criteria.
int group_notifications(struct mako_state *state, struct mako_criteria *criteria) {
	struct wl_list matches = {0};
	wl_list_init(&matches);

	// Now we're going to find all of the matching notifications and stick
	// them in a different list. Removing the first one from the global list
	// is technically unnecessary, since it will go back in the same place, but
	// it makes the rest of this logic nicer.
	struct wl_list *location = NULL;  // The place we're going to reinsert them.
	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		if (!match_criteria(criteria, notif)) {
			continue;
		}

		if (!location) {
			location = notif->link.prev;
		}

		wl_list_remove(&notif->link);
		wl_list_insert(&matches, &notif->link);
	}

	// Now we need to rematch criteria for all of the grouped notifications,
	// in case it changes their styles.
	wl_list_for_each(notif, &matches, link) {
		int rematch_count = apply_each_criteria(&state->config.criteria, notif);
		if (rematch_count == -1) {
			// We encountered an allocation failure or similar while applying
			// criteria. The notification may be partially matched, but the
			// worst case is that it has an empty style, so bail.
			fprintf(stderr, "Failed to apply criteria\n");
			return -1;
		} else if (rematch_count == 0) {
			// This should be impossible, since the global criteria is always
			// present in a mako_config and matches everything.
			fprintf(stderr, "Notification matched zero criteria?!\n");
			return -1;
		}
	}

	int count = wl_list_length(&matches);

	// Place all of the matches back into the list where the first one was
	// originally.
	wl_list_insert_list(location, &matches);

	return count;
}
