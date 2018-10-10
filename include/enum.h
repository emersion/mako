#ifndef _ENUM_H_
#define _ENUM_H_

// State is intended to work as a bitmask, so if more need to be added in the
// future, this should be taken into account.
enum mako_parse_state {
	MAKO_PARSE_STATE_NORMAL = 0,
	MAKO_PARSE_STATE_ESCAPE = 1,
	MAKO_PARSE_STATE_QUOTE = 2,
	MAKO_PARSE_STATE_QUOTE_ESCAPE = 3,
	MAKO_PARSE_STATE_FORMAT = 4,
};

#endif
