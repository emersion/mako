#ifndef MAKO_CRITERIA_H
#define MAKO_CRITERIA_H

#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "config.h"
#include "types.h"

struct mako_config;
struct mako_notification;

enum operator { OP_NONE, OP_EQUALS, OP_REGEX_MATCHES, OP_NOT_EQUALS, OP_TRUTHY, OP_FALSEY };

struct mako_condition {
	enum operator operator;
	char *value;
	regex_t pattern;
};

struct mako_criteria {
	struct mako_criteria_spec spec;
	struct wl_list link; // mako_config::criteria

	char *raw_string; // For debugging

	// Style to apply to matches:
	struct mako_style style;

	// Fields that can be matched:
	struct mako_condition app_name;
	struct mako_condition app_icon;
	bool actionable;  // Whether mako_notification.actions is nonempty
	bool expiring;  // Whether mako_notification.requested_timeout is non-zero
	enum mako_notification_urgency urgency;
	struct mako_condition category;
	struct mako_condition desktop_entry;
	struct mako_condition summary;
	struct mako_condition body;

	char *mode;

	// Second-pass matches:
	int group_index;
	bool grouped;  // Whether group_index is non-zero
	char *output;
	uint32_t anchor;
	bool hidden;
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

bool validate_criteria(struct mako_criteria *criteria);

#endif
