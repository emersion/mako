#ifndef _MAKO_WAYLAND_H
#define _MAKO_WAYLAND_H

#include <stdbool.h>

struct mako_state;

bool init_wayland(struct mako_state *state);
void finish_wayland(struct mako_state *state);

bool init_wayland_notification(struct mako_notification *notif);
void finish_wayland_notification(struct mako_notification *notif);

#endif
