#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <pango/pangocairo.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h>

#include "config.h"
#include "criteria.h"
#include "dbus.h"
#include "event-loop.h"
#include "mako.h"
#include "notification.h"
#include "icon.h"
#include "string-util.h"
#include "wayland.h"

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
	free(notif->tag);
	if (notif->image_data != NULL) {
		free(notif->image_data->data);
		free(notif->image_data);
	}

	notif->app_name = strdup("");
	notif->app_icon = strdup("");
	notif->summary = strdup("");
	notif->body = strdup("");
	notif->category = strdup("");
	notif->desktop_entry = strdup("");
	notif->tag = strdup("");

	notif->image_data = NULL;

	destroy_icon(notif->icon);
	notif->icon = NULL;
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
	wl_list_init(&notif->link);
	reset_notification(notif);

	// Start ungrouped.
	notif->group_index = -1;

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
		destroy_criteria(notif_criteria);
	}

	if (!notif->style.history ||
		notif->state->config.max_history <= 0) {
		destroy_notification(notif);
		return;
	}

	destroy_timer(notif->timer);
	notif->timer = NULL;

	wl_list_insert(&notif->state->history, &notif->link);
	while (wl_list_length(&notif->state->history) >
		notif->state->config.max_history) {
		struct mako_notification *n =
			wl_container_of(notif->state->history.prev, n, link);
		destroy_notification(n);
	}
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

struct mako_notification *get_tagged_notification(struct mako_state *state,
		const char *tag, const char *app_name) {
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->tag && strlen(notif->tag) != 0 &&
			strcmp(notif->tag, tag) == 0 &&
			strcmp(notif->app_name, app_name) == 0) {
			return notif;
		}
	}
	return NULL;
}

void close_group_notifications(struct mako_notification *top_notif,
	       enum mako_notification_close_reason reason) {
	struct mako_state *state = top_notif->state;

	if (top_notif->style.group_criteria_spec.none) {
		// No grouping, just close the notification
		close_notification(top_notif, reason);
		return;
	}

	struct mako_criteria *notif_criteria = create_criteria_from_notification(
		top_notif, &top_notif->style.group_criteria_spec);

	struct mako_notification *notif, *tmp;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		if (match_criteria(notif_criteria, notif)) {
			close_notification(notif, reason);
		}
	}

	destroy_criteria(notif_criteria);
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

// Any new format specifiers must also be added to VALID_FORMAT_SPECIFIERS.

char *format_hidden_text(char variable, bool *markup, void *data) {
	struct mako_hidden_format_data *format_data = data;
	switch (variable) {
	case 'h':
		return mako_asprintf("%zu", format_data->hidden);
	case 't':
		return mako_asprintf("%zu", format_data->count);
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
	case 'g':
		return mako_asprintf("%d", notif->group_count);
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

static const struct mako_binding *get_button_binding(struct mako_style *style,
		uint32_t button) {
	switch (button) {
	case BTN_LEFT:
		return &style->button_bindings.left;
	case BTN_RIGHT:
		return &style->button_bindings.right;
	case BTN_MIDDLE:
		return &style->button_bindings.middle;
	}
	return NULL;
}

void notification_execute_binding(struct mako_notification *notif,
		const struct mako_binding *binding,
		const struct mako_binding_context *ctx) {
	switch (binding->action) {
	case MAKO_BINDING_NONE:
		break;
	case MAKO_BINDING_DISMISS:
		close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BINDING_DISMISS_GROUP:
		close_group_notifications(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BINDING_DISMISS_ALL:
		close_all_notifications(notif->state, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BINDING_INVOKE_DEFAULT_ACTION:;
		struct mako_action *action;
		wl_list_for_each(action, &notif->actions, link) {
			if (strcmp(action->key, DEFAULT_ACTION_KEY) == 0) {
				char *activation_token = create_xdg_activation_token(
					ctx->surface, ctx->seat, ctx->serial);
				notify_action_invoked(action, activation_token);
				free(activation_token);
				break;
			}
		}
		close_notification(notif, MAKO_NOTIFICATION_CLOSE_DISMISSED);
		break;
	case MAKO_BINDING_EXEC:
		assert(binding->command != NULL);
		pid_t pid = fork();
		if (pid < 0) {
			perror("fork failed");
			break;
		} else if (pid == 0) {
			// Double-fork to avoid SIGCHLD issues
			pid = fork();
			if (pid < 0) {
				perror("fork failed");
				_exit(1);
			} else if (pid == 0) {
				char *const argv[] = { "sh", "-c", binding->command, NULL };
				execvp("sh", argv);
				perror("exec failed");
				_exit(1);
			}
			_exit(0);
		}
		if (waitpid(pid, NULL, 0) < 0) {
			perror("waitpid failed");
		}
		break;
	}
}

void notification_handle_button(struct mako_notification *notif, uint32_t button,
		enum wl_pointer_button_state state,
		const struct mako_binding_context *ctx) {
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}

	const struct mako_binding *binding =
		get_button_binding(&notif->style, button);
	if (binding != NULL) {
		notification_execute_binding(notif, binding, ctx);
	}
}

void notification_handle_touch(struct mako_notification *notif,
		const struct mako_binding_context *ctx) {
	notification_execute_binding(notif, &notif->style.touch_binding, ctx);
}

/*
 * Searches through the notifications list and returns the next position at
 * which to insert. If no results for the specified urgency are found,
 * it will return the closest link searching in the direction specified.
 * (-1 for lower, 1 or upper).
 */
static struct wl_list *get_last_notif_by_urgency(struct wl_list *notifications,
		enum mako_notification_urgency urgency, int direction) {
	enum mako_notification_urgency current = urgency;

	if (wl_list_empty(notifications)) {
		return notifications;
	}

	while (current <= MAKO_NOTIFICATION_URGENCY_CRITICAL &&
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
	struct mako_notification *notif = NULL, *tmp = NULL;
	size_t count = 0;
	wl_list_for_each_safe(notif, tmp, &state->notifications, link) {
		if (!match_criteria(criteria, notif)) {
			continue;
		}

		if (!location) {
			location = notif->link.prev;
		}

		wl_list_remove(&notif->link);
		wl_list_insert(matches.prev, &notif->link);
		notif->group_index = count++;
	}

	// If count is zero, we don't need to worry about changing anything. The
	// notification's style has its grouping criteria set to none.

	if (count == 1) {
		// If we matched a single notification, it means that it has grouping
		// criteria set, but didn't have any others to group with. This makes
		// it ungrouped just as if it had no grouping criteria. If this is a
		// new notification, its index is already set to -1. However, this also
		// happens when a notification had been part of a group and all the
		// others have closed, so we need to set it anyway.
		// We can't use the current pointer, wl_list_for_each_safe clobbers it.
		notif = wl_container_of(matches.prev, notif, link);
		notif->group_index = -1;
	}

	// Now we need to rematch criteria for all of the grouped notifications,
	// in case it changes their styles. We also take this opportunity to record
	// the total number of notifications in the group, so that it can be used
	// in the notifications' format.
	// We can't skip this even if there was only a single match, as we may be
	// removing the second-to-last notification of a group, and still need to
	// potentially change style now that the matched one isn't in a group
	// anymore.
	wl_list_for_each(notif, &matches, link) {
		notif->group_count = count;
	}

	// Place all of the matches back into the list where the first one was
	// originally.
	wl_list_insert_list(location, &matches);

	// We don't actually re-apply criteria here, that will happen just before
	// we render each notification anyway.

	return count;
}
