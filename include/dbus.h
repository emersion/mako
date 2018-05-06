#ifndef _MAKO_DBUS_H
#define _MAKO_DBUS_H

#include <stdbool.h>

struct mako_state;
struct mako_notification;
enum mako_notification_close_reason;

bool init_dbus(struct mako_state *state);
void finish_dbus(struct mako_state *state);
void notify_notification_closed(struct mako_notification *notif,
	enum mako_notification_close_reason reason);

#endif
