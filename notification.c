#include <stdlib.h>
#include <stdio.h>

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
