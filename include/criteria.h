#ifndef MAKO_CRITERIA_H
#define MAKO_CRITERIA_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "config.h"
#include "types.h"

struct mako_config;
struct mako_notification;

struct mako_criteria {
	struct mako_criteria_spec spec;
	struct wl_list link; // mako_config::criteria

	char *raw_string; // For debugging

	// Style to apply to matches:
	struct mako_style style;

	// Fields that can be matched:
	char *app_name;
	char *app_icon;
	bool actionable;  // Whether mako_notification.actions is nonempty
	bool expiring;  // Whether mako_notification.requested_timeout is non-zero
	enum mako_notification_urgency urgency;
	char *category;
	char *desktop_entry;
	regex_t summary;
	char *body;
	int group_index;
	bool grouped;  // Whether group_index is non-zero
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
