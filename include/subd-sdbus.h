#ifndef _SUBD_SDBUS_H
#define _SUBD_SDBUS_H

#include <dbus/dbus.h>
#include <wayland-util.h>

#define SD_BUS_NAME_ALLOW_REPLACEMENT DBUS_NAME_FLAG_ALLOW_REPLACEMENT
#define SD_BUS_NAME_REPLACE_EXISTING DBUS_NAME_FLAG_REPLACE_EXISTING
#define SD_BUS_NAME_QUEUE ~DBUS_NAME_FLAG_DO_NOT_QUEUE

/**
 * These types are the rough equivalents of each other in libdbus and sd-bus.
 * They are opaque structures in both library, so we can just typedef them to
 * make our life easier.
 */
typedef struct DBusConnection sd_bus;
typedef struct DBusError sd_bus_error;
typedef struct subd_member sd_bus_vtable;

/**
 * his is not used, but typedef-d to preserve sd-bus function signatures.
 */
typedef int sd_bus_slot;

/**
 * In sd-bus, sd_bus_message knows what sd_bus it is associated with, and also
 * keeps track of its current read/write position. This can be achieved with
 * libdbus by encapsulating the DBusConnection (sd_bus), the DBusMessage, and a
 * stack of iterators (implemented here as a wl_list of msg_iter structs) in one
 * structure. The current DBusMessageIter (iter) is also stored to make
 * accessing it easier (i.e. it's just syntactic sugar).
 */
struct msg_iter {
	DBusMessageIter iter;
	struct wl_list link;
};

typedef struct sd_bus_message {
	sd_bus *bus;
	DBusMessage *message;
	DBusMessageIter *iter;
	struct wl_list *iters;
	int ref_count;
} sd_bus_message;

/**
 * The following two enums and one struct are pulled straight from subd, and are
 * used by the handler dispatcher logic, and the automatic Introspectable
 * intarface implementation.
 *
 * TODO: These could be changed to behave the same as sd-bus vtables, which
 * would eliminate the need of #ifdef-d vtable definitions in dbus/xdg.c and
 * dbus/mako.c.
 */

/**
 * Represents the three DBus member types.
 */
enum subd_member_type {
	SUBD_METHOD,
	SUBD_SIGNAL,
	SUBD_PROPERTY,
	SUBD_MEMBERS_END,
};

/**
 * Represents the three possible access types for DBus properties.
 */
enum subd_property_access {
	SUBD_PROPERTY_READ,
	SUBD_PROPERTY_WRITE,
	SUBD_PROPERTY_READWRITE,
};

/**
 * This struct represents a DBus object member, and serves two purpose:
 *
 *  - If the member is a method, it has a pointer to the method's handler
 *    function, so that the vtable dispatcher knows where to dispatch execution.
 *  - It has the metadata for the member (name, output and/or input signatures,
 *    access) that introspection data can be built from.
 */
struct subd_member {
	enum subd_member_type type;
	union {
		struct {
			const char *name;
			int (*handler)(sd_bus_message *, void *, sd_bus_error *);
			const char *input_signature;
			const char *output_signature;
		} m;
		struct {
			const char *name;
			const char *signature;
		} s;
		struct {
			const char *name;
			const char *signature;
			enum subd_property_access access;
		} p;
		int e;
	};
};

/**
 * The following functions are wrappers around the libdbus library that mimic
 * their sd-bus equivalents. These are not necessarily 1:1 matches of the
 * originals, only the functionality needed for mako is implemented here.
 * Most notable difference is that functions that read or write messages are
 * only able to deal with basic types.
 */

int sd_bus_open_user(sd_bus **bus);
int sd_bus_request_name(sd_bus *bus, const char *name, int flags);
void sd_bus_slot_unref(int *slot);
void sd_bus_flush_close_unref(sd_bus *bus);

void sd_bus_error_set_const(sd_bus_error *err, const char *name, const char *msg);

int sd_bus_message_new_method_return(sd_bus_message *msg, sd_bus_message **newmsg);
int sd_bus_message_open_container(sd_bus_message *msg, char type,
	const char *signature);
int sd_bus_message_append(sd_bus_message *msg, const char *signature, ...);
int sd_bus_message_close_container(sd_bus_message *msg);
int sd_bus_message_peek_type(sd_bus_message *msg, void *unused,
	const char **signature);
int sd_bus_message_read(sd_bus_message *msg, const char *signature, ...);
int sd_bus_message_enter_container(sd_bus_message *msg, char type,
	const char *signature);
int sd_bus_message_exit_container(sd_bus_message *msg);
int sd_bus_message_skip(sd_bus_message *msg, const char *signature);
void sd_bus_message_unref(sd_bus_message *msg);

int sd_bus_emit_signal(sd_bus *bus, const char *path, const char *interface,
	const char *name, const char *signature, ...);
int sd_bus_reply_method_return(sd_bus_message *msg, const char *signature, ...);
int sd_bus_send(sd_bus *bus, sd_bus_message *msg, dbus_uint32_t *cookie);

int sd_bus_add_object_vtable(sd_bus *bus, sd_bus_slot **slot, const char *path_name,
	const char *interface_name, const sd_bus_vtable *vtable, void *userdata);

#endif
