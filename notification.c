#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus.h"
#include "mako.h"
#include "notification.h"

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
	notif->urgency = MAKO_NOTIFICATION_URGENCY_UNKNWON;
	wl_list_insert(&state->notifications, &notif->link);
	return notif;
}

void destroy_notification(struct mako_notification *notif) {
	wl_list_remove(&notif->link);
	struct mako_action *action, *tmp;
	wl_list_for_each_safe(action, tmp, &notif->actions, link) {
		wl_list_remove(&action->link);
		free(action->id);
		free(action->title);
		free(action);
	}
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

static size_t trim_space(char *dst, const char *src, size_t src_len) {
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

size_t format_notification(struct mako_notification *notif, const char *format,
		char *buf) {
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

		const char *value = NULL;
		switch (current[1]) {
		case '%':
			value = "%";
			break;
		case 'a':
			value = notif->app_name;
			break;
		case 's':
			value = notif->summary;
			break;
		case 'b':
			value = notif->body;
			break;
		}
		if (value == NULL) {
			value = "";
		}

		size_t value_len = strlen(value);
		if (buf != NULL) {
			memcpy(buf + len, value, value_len);
		}
		len += value_len;

		last = current + 2;
	}

	if (buf != NULL) {
		trim_space(buf, buf, len);
	}
	return len;
}
