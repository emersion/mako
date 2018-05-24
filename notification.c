#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif

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
	notif->urgency = MAKO_NOTIFICATION_URGENCY_UNKNOWN;
	wl_list_insert(&state->notifications, &notif->link);
	return notif;
}

void destroy_notification(struct mako_notification *notif) {
	wl_list_remove(&notif->link);
	struct mako_action *action, *tmp;
	wl_list_for_each_safe(action, tmp, &notif->actions, link) {
		wl_list_remove(&action->link);
		free(action->key);
		free(action->title);
		free(action);
	}
	destroy_timer(notif->timer);
	free(notif->app_name);
	free(notif->app_icon);
	free(notif->summary);
	free(notif->body);
	free(notif->category);
	free(notif->desktop_entry);
	free(notif);
}

void close_notification(struct mako_notification *notif,
		enum mako_notification_close_reason reason) {
	notify_notification_closed(notif, reason);
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

char *format_state_text(char variable, void *data) {
	struct mako_state *state = (struct mako_state *)data;
	char *value = NULL;
	switch (variable) {
	case 'h':;
		int hidden = wl_list_length(&state->notifications) - state->config.max_visible;
		size_t hidden_ln = snprintf(NULL, 0, "%d", hidden);
		value = malloc(hidden_ln + 1);
		if(!value) {
			break;
		}
		snprintf(value, hidden_ln + 1, "%d", hidden);
		return value;
	case 't':;
		int count = wl_list_length(&state->notifications);
		size_t total_ln = snprintf(NULL, 0, "%d", count);
		value = malloc(total_ln + 1);
		if(!value) {
			break;
		}
		snprintf(value, total_ln + 1, "%d", count);
		return value;
	}
	return value;
}

char* format_notif_text(char variable, void *data) {
	struct mako_notification *notif = (struct mako_notification *)data;
	char *value = NULL;
	switch (variable) {
	case 'a':
		value = strdup(notif->app_name);
		break;
	case 's':
		value = strdup(notif->summary); 
		break;
	case 'b':
		value = strdup(notif->body);
		break;
	}
	return value;
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
			value = "%";
		} else {
			value =	format_func(current[1], data);
		}
		if (value == NULL) {
			value = "";
		}

		size_t value_len;
		if (!markup) {
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
