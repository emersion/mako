#ifndef MAKO_TYPES_H
#define MAKO_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <cairo.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct mako_color {
	uint32_t value;
	cairo_operator_t operator;
};

bool parse_boolean(const char *string, bool *out);
bool parse_int(const char *string, int *out);
bool parse_int_ge(const char *string, int *out, int min);
bool parse_color(const char *string, uint32_t *out);
bool parse_mako_color(const char *string, struct mako_color *out);
bool parse_anchor(const char *string, uint32_t *out);

enum mako_notification_urgency {
	MAKO_NOTIFICATION_URGENCY_LOW = 0,
	MAKO_NOTIFICATION_URGENCY_NORMAL = 1,
	MAKO_NOTIFICATION_URGENCY_CRITICAL = 2,
	MAKO_NOTIFICATION_URGENCY_UNKNOWN = -1,
};

bool parse_urgency(const char *string, enum mako_notification_urgency *out);

struct mako_directional {
	int32_t top;
	int32_t right;
	int32_t bottom;
	int32_t left;
};

bool parse_directional(const char *string, struct mako_directional *out);

// Criteria specifications are used for two things.
// Primarily, they keep track of whether or not each field was part of the a
// criteria specification, so that, for example, "not actionable" can be
// distinguished from "don't care".
// Additionally, they are used to store the set of criteria that must match for
// notifications to group with each other.
struct mako_criteria_spec {
	bool app_name;
	bool app_icon;
	bool actionable;
	bool expiring;
	bool urgency;
	bool category;
	bool desktop_entry;
	bool sound_name;
	bool summary;
	bool summary_pattern;
	bool body;
	bool body_pattern;

	bool mode;

	bool none; // Special criteria that never matches, used for grouping

	// Fields that can only be matched after grouping, and thus can't be
	// used to group.
	bool group_index;
	bool grouped;
	bool output;
	bool anchor;
	bool hidden;
};

bool parse_criteria_spec(const char *string, struct mako_criteria_spec *out);
bool mako_criteria_spec_any(const struct mako_criteria_spec *spec);

// List of specifier characters that can appear in a format string.
extern const char VALID_FORMAT_SPECIFIERS[];

bool parse_format(const char *string, char **out);

#endif
