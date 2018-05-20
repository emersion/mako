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
	return false;
}
