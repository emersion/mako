#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "subd.h"

struct vtable_userdata {
	struct wl_list *interfaces;
	void *userdata;
};

struct interface {
	const char *name;
	const struct subd_member *members;
	struct wl_list link;
};

struct path {
	const char *path;
	char *introspection_data;
	struct wl_list *interfaces;
	struct wl_list link;
};

static struct wl_list *paths = NULL;

static bool add_args(FILE *stream, const char *sig, const char *end) {
	DBusSignatureIter iter;
	if (!dbus_signature_validate(sig, NULL)) {
		return false;
	}

	if (strlen(sig) > 0) {
		dbus_signature_iter_init(&iter, sig);
		do {
			char *s = dbus_signature_iter_get_signature(&iter);
			fprintf(stream, "   <arg type=\"%s\" %s />\n", s, end);
			dbus_free(s);
		} while (dbus_signature_iter_next(&iter));
	}

	return true;
}

static const char *generate_introspection_data(struct path *path) {
	free(path->introspection_data);

	size_t size;
	FILE *stream = open_memstream(&path->introspection_data, &size);
	if (stream == NULL) {
		return NULL;
	}

	// Write the DOCTYPE entity, and start the <node> element.
	fputs(DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE, stream);
	fputs("<node>\n", stream);

	// Iterate through the path's interfaces, ...
	struct interface *interface;
	wl_list_for_each(interface, path->interfaces, link) {
		fprintf(stream, " <interface name=\"%s\">\n", interface->name);

		const struct subd_member *member = interface->members;
		while (member->type != SUBD_MEMBERS_END) {
			switch (member->type) {
			case SUBD_METHOD:
				fprintf(stream, "  <method name=\"%s\">\n", member->m.name);
				add_args(stream, member->m.input_signature, "direction=\"in\"");
				add_args(stream, member->m.output_signature, "direction=\"out\"");
				fprintf(stream, "  </method>\n");
				break;
			case SUBD_SIGNAL:
				fprintf(stream, "  <signal name=\"%s\">\n", member->s.name);
				add_args(stream, member->s.signature, "");
				fprintf(stream, "  </signal>\n");
				break;
			case SUBD_PROPERTY:
				fprintf(stream, "  <property name=\"%s\" type=\"%s\" ",
					member->p.name, member->p.signature);
				switch (member->p.access) {
				case SUBD_PROPERTY_READ:
					fprintf(stream, "\"read\" />\n");
					break;
				case SUBD_PROPERTY_WRITE:
					fprintf(stream, "\"write\" />\n");
					break;
				case SUBD_PROPERTY_READWRITE:
					fprintf(stream, "\"readwrite\" />\n");
					break;
				}
				break;
			case SUBD_MEMBERS_END:
				// not gonna happen
				break;
			}
			member++;
		}
		fprintf(stream, " </interface>\n");
	}
	fprintf(stream, "</node>");
	fclose(stream);

	return path->introspection_data;
}

static int handle_introspect(sd_bus_message *msg, void *data, sd_bus_error *err) {
	// Find the path the message was sent to, so we can access its list of
	// implemented interfaces.
	const char *path_name = dbus_message_get_path(msg->message);
	struct path *p, *path = NULL;
	wl_list_for_each(p, paths, link) {
		if (strcmp(p->path, path_name) == 0) {
			path = p;
			break;
		}
	}

	if (path == NULL) {
		// Something is seriously weird here. Path was not found, but then how
		// was the method handler called??
		dbus_set_error(err, DBUS_ERROR_INVALID_ARGS,
			"Path was not found in paths list.");
		return -EINVAL;
	}

	if (path->introspection_data == NULL) {
		// This shouldn't really happen, introspection data is generated when
		// the vtable is registered.
		if (generate_introspection_data(path) == NULL) {
			dbus_set_error(err, DBUS_ERROR_NO_MEMORY,
				"Introspection data could not be generated.");
			return -ENOMEM;
		}
	}

	return sd_bus_reply_method_return(msg, "s", path->introspection_data);
}

static const struct subd_member introspectable_members[] = {
	{SUBD_METHOD, .m = {"Introspect", handle_introspect, "", "s"}},
	{SUBD_MEMBERS_END, .e=0},
};

/**
 * Helper function for vtable_dispatch that returns wither NULL, or the method
 * object member with the name "name".
 */
static const struct subd_member *find_member(const struct subd_member *members,
		const char *name) {
	const struct subd_member *member = members;
	while (member->type != SUBD_MEMBERS_END) {
		if (member->type == SUBD_METHOD && strcmp(member->m.name, name) == 0) {
			return member;
		}
		++member;
	}
	return NULL;
}

/**
 * Helper function for vtable_dispatch that calls the method object member's
 * handler function, and sends an error message if necessary.
 */
static bool call_method(const struct subd_member *member, DBusConnection *conn,
		DBusMessage *msg, void *userdata) {
	sd_bus_message *message = NULL;
	message = malloc(sizeof(sd_bus_message));
	if (message == NULL) {
		return false;
	}
	message->bus = conn;
	message->message = msg;
	message->ref_count = 1;
	message->iters = malloc(sizeof(struct wl_list));
	if (message->iters == NULL) {
		return false;
	}
	wl_list_init(message->iters);

	struct msg_iter *iter = malloc(sizeof(struct msg_iter));
	if (iter == NULL) {
		return false;
	}
	dbus_message_iter_init(message->message, &iter->iter);
	wl_list_insert(message->iters, &iter->link);
	message->iter = &iter->iter;

	DBusError error;
	dbus_error_init(&error);
	if (member->m.handler(message, userdata, &error) < 0) {
		if (dbus_error_is_set(&error)) {
			DBusMessage *error_message =
				dbus_message_new_error(msg, error.name, error.message);
			dbus_connection_send(conn, error_message, 0);
			dbus_error_free(&error);
		}
		return false;
	}
	return true;
}

/**
 * This function receives a list of interfaces implemented by the destination
 * object in "userdata->interfaces", and iterates through them and their members
 * to find the called method. When the method is found, its handler function is
 * called with "data" set to "userdata->userdata".
 */
static DBusHandlerResult vtable_dispatch(DBusConnection *connection,
		DBusMessage *message, void *userdata) {
	struct vtable_userdata *data = userdata;
	const char *interface_name = dbus_message_get_interface(message);
	const char *member_name = dbus_message_get_member(message);
	if (interface_name == NULL || member_name == NULL) {
		// something is wrong, no need to try with other handlers
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	struct interface *interface;
	wl_list_for_each(interface, data->interfaces, link) {
		if (strcmp(interface->name, interface_name) == 0) {
			const struct subd_member *member =
				find_member(interface->members, member_name);
			if (member != NULL) {
				call_method(member, connection, message, data->userdata);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable dbus_vtable = {
	.message_function = vtable_dispatch,
	.unregister_function = NULL,
};

static struct path *register_new_path(sd_bus * bus, const char *path_name,
		void *userdata) {
	struct wl_list *interfaces = NULL;
	struct interface *new_interface = NULL;
	struct path *new_path = NULL;
	struct vtable_userdata *data = NULL;

	// We create an interface list, and append Introspectable to it. We want
	// every path to implement org.freedesktop.DBus.Introspectable.
	// TODO: Also implement the following:
	//        - org.freedesktop.DBus.Peer
	//        - org.freedesktop.DBus.Properties
	interfaces = malloc(sizeof(struct wl_list));
	if (interfaces == NULL) {
		goto error;
	}
	wl_list_init(interfaces);

	new_interface = malloc(sizeof(struct interface));
	if (new_interface == NULL) {
		goto error;
	}

	new_interface->name = strdup("org.freedesktop.DBus.Introspectable");
	new_interface->members = introspectable_members;
	wl_list_insert(interfaces, &new_interface->link);

	// Create the path with the new interface list
	new_path = malloc(sizeof(struct path));
	if (new_path == NULL) {
		goto error;
	}

	new_path->path = strdup(path_name);
	new_path->interfaces = interfaces;
	new_path->introspection_data = NULL; // This will be set later.

	// ... and also register it.
	// We merge userdata with the interfaces list in a struct
	// vtable_userdata here, so vtable_dispatch will know what interfaces to
	// traverse, and what userdata to pass to the actual handler functions.
	data = malloc(sizeof(struct vtable_userdata));
	if (data == NULL) {
		goto error;
	}

	data->interfaces = interfaces;
	data->userdata = userdata;
	if (!dbus_connection_try_register_object_path(bus, path_name, &dbus_vtable,
			data, NULL)) {
		goto error;
	}

	// Append the path to the list of paths
	wl_list_insert(paths, &new_path->link);

	return new_path;

error:
	free(data);
	free(new_path);
	free(new_interface);
	free(interfaces);
	return NULL;
}

int sd_bus_add_object_vtable(sd_bus *bus, sd_bus_slot **slot,
		const char *path_name, const char *interface_name,
		const sd_bus_vtable *vtable, void *userdata) {

	if (paths == NULL) {
		paths = malloc(sizeof(struct path));
		if (paths == NULL) {
			return -ENOMEM;
		}
		wl_list_init(paths);
	}

	// See if this path is already registered, ...
	struct path *p, *path = NULL;
	wl_list_for_each(p, paths, link) {
		if (strcmp(path_name, p->path) == 0) {
			path = p;
			break;
		}
	}

	// ... and if it is not, register it.
	if (path == NULL && 
			(path = register_new_path(bus, path_name, userdata)) == NULL) {
		return -ENOMEM;
	}

	// See if the path already has an interface with this name, ...
	struct interface *i, *interface = NULL;
	wl_list_for_each(i, path->interfaces, link) {
		if (strcmp(i->name, interface_name) == 0) {
			interface = i;
			break;
		}
	}

	// ... and create it, if not. If it does, replace the vtable for the
	// existing interface.
	if (interface == NULL) {
		interface = malloc(sizeof(struct interface));
		if (interface == NULL) {
			return -ENOMEM;
		}
		interface->name = strdup(interface_name);
		interface->members = vtable;
		wl_list_insert(path->interfaces, &interface->link);
	} else {
		interface->members = vtable;
	}

	// (Re)generate introspection XML for this path.
	generate_introspection_data(path);

	return 0;
}
