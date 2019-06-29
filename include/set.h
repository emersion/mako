#ifndef MAKO_SET_H
#define MAKO_SET_H

#include <stdbool.h>
#include <stdint.h>

struct mako_set_top {
	struct mako_set_second *tables[0xFF];
};

struct mako_set_second {
	struct mako_set_third *tables[0xFF];
};

struct mako_set_third {
	struct mako_set_last *tables[0xFF];
};

struct mako_set_last {
	bool values[0xFF];
};

/// Returns true if the set contains the value.
bool mako_set_contains(struct mako_set_top *top, uint32_t value);
/// Inserts the value into the set, allocating tables as needed.
void mako_set_insert(struct mako_set_top **top, uint32_t value);
/// Removes the value from the set, returns true if the value existed.
bool mako_set_remove(struct mako_set_top *top, uint32_t value);
/// Shrinks the set by deallocating empty tables.
void mako_set_shrink(struct mako_set_top **top);

#endif