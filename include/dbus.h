#ifndef _MAKO_DBUS_H
#define _MAKO_DBUS_H

#include <stdbool.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#endif

struct mako_state;
struct mako_notification;
struct mako_action;
enum mako_notification_close_reason;

bool init_dbus(struct mako_state *state);
void finish_dbus(struct mako_state *state);
void notify_notification_closed(struct mako_notification *notif,
	enum mako_notification_close_reason reason);

void notify_action_invoked(struct mako_action *action);

int init_dbus_xdg(struct mako_state *state);

int init_dbus_mako(struct mako_state *state);

#endif
