#ifndef _MAKO_TYPES_H
#define _MAKO_TYPES_H

#include <stdbool.h>
#include <stdint.h>

bool parse_boolean(const char *string, bool *out);
bool parse_int(const char *string, int *out);
bool parse_color(const char *string, uint32_t *out);

enum mako_notification_urgency {
	MAKO_NOTIFICATION_URGENCY_LOW = 0,
	MAKO_NOTIFICATION_URGENCY_NORMAL = 1,
	MAKO_NOTIFICATION_URGENCY_HIGH = 2,
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

// List of specifier characters that can appear in a format string.
static const char VALID_FORMAT_SPECIFIERS[] = "%asbht";

bool parse_format(const char *string, char **out);

#endif
