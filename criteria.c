#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>
#include "mako.h"
#include "criteria.h"
#include "notification.h"

struct mako_criteria *create_criteria(struct mako_state *state) {
	struct mako_criteria *criteria = calloc(1, sizeof(struct mako_criteria));
	if (criteria == NULL) {
		fprintf(stderr, "allocation failed\n");
		return NULL;
	}

	// Copy the global configuration, which will then be overridden as needed
	// during the configuration of this criteria.
	*criteria->config = state->config;
	return criteria;
}

void finish_criteria(struct mako_criteria *criteria) {
	free(criteria->app_name);
	free(criteria->app_icon);
	free(criteria->category);
	free(criteria->desktop_entry);
}

bool match_criteria(struct mako_criteria *criteria,
		struct mako_notification *notif) {
	struct mako_criteria_spec spec = criteria->specified;

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
