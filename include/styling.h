#ifndef MAKO_STYLING_H
#define MAKO_STYLING_H

#include "config.h"
#include "notification.h"

void init_default_style(struct mako_style *style);
void init_empty_style(struct mako_style *style);
void finish_style(struct mako_style *style);
bool apply_style(struct mako_notification *notif, const struct mako_style *style);
bool apply_superset_style(
		struct mako_style *target, struct mako_config *config);

#endif
