#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>

#include "set.h"

bool mako_set_contains(struct mako_set_top *top, uint32_t value) {
	if (top == NULL) {
		return false;
	}

	struct mako_set_second *second = top->tables[value & 0xFF];
	if (second == NULL) {
		return false;
	}

	struct mako_set_third *third = second->tables[(value >> 8) & 0xFF];
	if (third == NULL) {
		return false;
	}

	struct mako_set_last *last = third->tables[(value >> 16) & 0xFF];
	if (last == NULL) {
		return false;
	}

	return last->values[(value >> 24) & 0xFF];
}

void mako_set_insert(struct mako_set_top **top, uint32_t value) {
	if (*top == NULL) {
		*top = (struct mako_set_top*) calloc(1, sizeof(struct mako_set_top));
	}

	struct mako_set_second **second = &((*top)->tables[value & 0xFF]);
	if (*second == NULL) {
		*second = (struct mako_set_second*) calloc(1, sizeof(struct mako_set_second));
	}

	struct mako_set_third **third = &((*second)->tables[(value >> 8) & 0xFF]);
	if (*third == NULL) {
		*third = (struct mako_set_third*) calloc(1, sizeof(struct mako_set_third));
	}

	struct mako_set_last **last = &((*third)->tables[(value >> 16) & 0xFF]);
	if (*last == NULL) {
		*last = (struct mako_set_last*) calloc(1, sizeof(struct mako_set_last));
	}

	(*last)->values[(value >> 24) & 0xFF] = true;
}

bool mako_set_remove(struct mako_set_top *top, uint32_t value) {
	if (top == NULL) {
		return false;
	}

	struct mako_set_second *second = top->tables[value & 0xFF];
	if (second == NULL) {
		return false;
	}

	struct mako_set_third *third = second->tables[(value >> 8) & 0xFF];
	if (third == NULL) {
		return false;
	}

	struct mako_set_last *last = third->tables[(value >> 16) & 0xFF];
	if (last == NULL) {
		return false;
	}

	bool contained = last->values[(value >> 24) & 0xFF];
	last->values[(value >> 24) & 0xFF] = false;

	return contained;
}

void mako_set_shrink_last(struct mako_set_last **last) {
	if (*last == NULL) 
		return;

	bool any_exists = false;
	for (uint8_t i = 0; i < 0xFF; i++) {
		if ((*last)->values[i]) {
			any_exists = true;
			break;
		}
	}

	if (!any_exists) {
		free(*last);
		*last = NULL;
	}
}

void mako_set_shrink_third(struct mako_set_third **third) {
	if (*third == NULL)
		return;

	for (uint8_t i = 0; i < 0xFF; i++) {
		mako_set_shrink_last(
			&(
				(*third)->tables[i]
			)
		);
	}

	bool any_exists = false;
	for (uint8_t i = 0; i < 0xFF; i++) {
		if ((*third)->tables[i] != NULL) {
			any_exists = true;
			break;
		}
	}

	if (!any_exists) {
		free(*third);
		*third = NULL;
	}
}

void mako_set_shrink_second(struct mako_set_second **second) {
	if (*second == NULL)
		return;

	for (uint8_t i = 0; i < 0xFF; i++) {
		mako_set_shrink_third(
			&(
				(*second)->tables[i]
			)
		);
	}

	bool any_exists = false;
	for (uint8_t i = 0; i < 0xFF; i++) {
		if ((*second)->tables[i] != NULL) {
			any_exists = true;
			break;
		}
	}

	if (!any_exists) {
		free(*second);
		*second = NULL;
	}
}

void mako_set_shrink(struct mako_set_top **top) {
	for (uint8_t i = 0; i < 0xFF; i++) {
		mako_set_shrink_second(
			&(
				(*top)->tables[i]
			)
		);
	}

	bool any_exists = false;
	for (uint8_t i = 0; i < 0xFF; i++) {
		if ((*top)->tables[i] != NULL) {
			any_exists = true;
			break;
		}
	}

	if (!any_exists) {
		free(*top);
		*top = NULL;
	}
}


