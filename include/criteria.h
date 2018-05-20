#ifndef _MAKO_CRITERIA_H
#define _MAKO_CRITERIA_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "notification.h"

struct mako_config;

// Stores whether or not each field was part of the criteria specification, so
// that, for example, "not actionable" can be distinguished from "don't care".
// This is unnecessary for string fields, but it's best to just keep it
// consistent.
struct mako_criteria_spec {
	bool app_name;
	bool app_icon;
	bool actionable;
	bool urgency;
	bool category;
	bool desktop_entry;
};

struct mako_criteria {
	struct mako_criteria_spec spec;

	// Style to apply to matches:
	struct mako_style style;

	// Fields that can be matched:
	char *app_name;
	char *app_icon;
	bool actionable; // Whether mako_notification.actions is nonempty

	enum mako_notification_urgency urgency;
	char *category;
	char *desktop_entry;
};

struct mako_criteria *create_criteria(struct mako_state *state);
void destroy_criteria(struct mako_criteria *criteria);
bool match_criteria(struct mako_criteria *criteria,
		struct mako_notification *notif);

#endif
