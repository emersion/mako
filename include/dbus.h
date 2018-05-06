#ifndef _MAKO_DBUS_H
#define _MAKO_DBUS_H

#include <stdbool.h>

struct mako_state;

bool init_dbus(struct mako_state *state);
void finish_dbus(struct mako_state *state);

#endif
