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
	bool expiring;
	bool urgency;
	bool category;
	bool desktop_entry;
	bool summary;
	bool body;
};

struct mako_criteria {
	struct mako_criteria_spec spec;
	struct wl_list link; // mako_config::criteria

	// Style to apply to matches:
	struct mako_style style;

	// Fields that can be matched:
	char *app_name;
	char *app_icon;
	bool actionable; // Whether mako_notification.actions is nonempty
	bool expiring; // Whether mako_notification.requested_timeout is non-zero
	enum mako_notification_urgency urgency;
	char *category;
	char *desktop_entry;
	char *summary;
	char *body;
};

struct mako_criteria *create_criteria(struct mako_config *config);
void destroy_criteria(struct mako_criteria *criteria);
bool match_criteria(struct mako_criteria *criteria,
		struct mako_notification *notif);

bool parse_criteria(const char *string, struct mako_criteria *criteria);
bool apply_criteria_field(struct mako_criteria *criteria, char *token);

struct mako_criteria *global_criteria(struct mako_config *config);
ssize_t apply_each_criteria(struct wl_list *criteria_list,
		struct mako_notification *notif);
struct mako_criteria *create_criteria_from_notification(
		struct mako_notification *notif, struct mako_criteria_spec *spec);

#endif
