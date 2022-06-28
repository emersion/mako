#ifndef MAKO_DBUS_H
#define MAKO_DBUS_H

#include <stdbool.h>
#if defined(HAVE_LIBSYSTEMD)
#include <systemd/sd-bus.h>
#elif defined(HAVE_LIBELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_BASU)
#include <basu/sd-bus.h>
#endif

struct mako_state;
struct mako_notification;
struct mako_action;
enum mako_notification_close_reason;

bool init_dbus(struct mako_state *state);
void finish_dbus(struct mako_state *state);
void notify_notification_closed(struct mako_notification *notif,
	enum mako_notification_close_reason reason);

void notify_action_invoked(struct mako_action *action,
	const char *activation_token);

int init_dbus_xdg(struct mako_state *state);

int init_dbus_mako(struct mako_state *state);

#endif
