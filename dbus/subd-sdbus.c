#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "subd.h"

int sd_bus_open_user(sd_bus **bus) {
	if (!dbus_threads_init_default()) {
		return -ENOMEM;
	}

	*bus = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (*bus == NULL) {
		return -ENXIO;
	}

	return 0;
}

int sd_bus_request_name(sd_bus *bus, const char *name, int flags) {
	if (dbus_bus_request_name(bus, name, flags, NULL) == -1) {
		return -EEXIST;
	}

	return 0;
}

void sd_bus_slot_unref(int *slot) {
	// not used
}

void sd_bus_flush_close_unref(sd_bus *bus) {
	// only flush and unref, no need to dbus_connection_close shared connections
	dbus_connection_flush(bus);
	dbus_connection_unref(bus);
}

void sd_bus_error_set_const(sd_bus_error *err, const char *name, const char *msg) {
	dbus_set_error_const(err, name, msg);
}

int sd_bus_message_new_method_return(sd_bus_message *msg, sd_bus_message **newmsg) {
	sd_bus_message *new_message = NULL;
	DBusMessage *message = NULL;
	struct msg_iter *iter = NULL;

	// Allocate a new "method return" DBusMessage
	if ((message = dbus_message_new_method_return(msg->message)) == NULL) {
		goto error;
	}

	// Allocate a new sd_bus_message
	new_message = malloc(sizeof(sd_bus_message));
	if (new_message == NULL) {
		goto error;
	}

	// Populate the fields of the new sd_bus_message so that it knows which bus
	// it is to be sent on, and what DBusMessage it contains.
	new_message->bus = msg->bus;
	new_message->message = message;
	new_message->ref_count = 1;

	//Also initialize the sd_bus_message's iterator stack, ...
	new_message->iters = malloc(sizeof(struct wl_list));
	if (new_message->iters == NULL) {
		goto error;
	}
	wl_list_init(new_message->iters);

	// ... and push a new append iterator to it.
	iter = malloc(sizeof(struct msg_iter));
	if (iter == NULL) {
		goto error;
	}
	dbus_message_iter_init_append(new_message->message, &iter->iter);
	wl_list_insert(new_message->iters, &iter->link);
	new_message->iter = &iter->iter;

	*newmsg = new_message;
	return 0;

error:
	free(iter);
	free(new_message->iters);
	free(new_message);
	dbus_message_unref(message);
	*newmsg = NULL;
	return -ENOMEM;
}

int sd_bus_message_open_container(sd_bus_message *msg, char type,
		const char *signature) {
	struct msg_iter *sub = malloc(sizeof(struct msg_iter));
	if (sub == NULL) {
		goto error;
	}

	if (!dbus_message_iter_open_container(msg->iter, type, signature, &sub->iter)) {
		goto error;
	}
	wl_list_insert(msg->iters, &sub->link);
	msg->iter = &sub->iter;

	return 0;

error:
	free(sub);
	return -ENOMEM;
}

int sd_bus_message_close_container(sd_bus_message *msg) {
	// Remove the iterator we want to close from from the iterator stack
	struct msg_iter *iter = wl_container_of(msg->iters->next, iter, link);
	wl_list_remove(&iter->link);

	// Set msg's current iterator to the top of the iterator stack.
	struct msg_iter *new_head = wl_container_of(msg->iters->next, iter, link);
	msg->iter = &new_head->iter;

	// Close the container.
	if (!dbus_message_iter_close_container(msg->iter, &iter->iter)) {
		return -ENOMEM;
	}

	return 0;
}

int sd_bus_message_append(sd_bus_message *msg, const char *signature, ...) {
	if (!dbus_signature_validate(signature, NULL)) {
		return -EINVAL;
	}

	va_list ap;
	DBusSignatureIter iter;
	va_start(ap, signature);
	dbus_signature_iter_init(&iter, signature);

	do {
		int type = dbus_signature_iter_get_current_type(&iter);
		if (!dbus_type_is_basic(type)) {
			va_end(ap);
			return -EINVAL;
		}

		DBusBasicValue val = va_arg(ap, DBusBasicValue);
		if (!dbus_message_iter_append_basic(msg->iter, type, &val)) {
			va_end(ap);
			return -ENOMEM;
		}
	} while(dbus_signature_iter_next(&iter));

	va_end(ap);
	return 0;
}

void sd_bus_message_unref(sd_bus_message *msg) {
	dbus_message_unref(msg->message);
	if (--msg->ref_count == 0) {
		struct msg_iter *iter, *itertmp = NULL;
		wl_list_for_each_safe(iter, itertmp, msg->iters, link) {
			free(iter);
		}
		free(msg->iters);
		free(msg);
	}
}

int sd_bus_message_peek_type(sd_bus_message *msg, void *unused,
		const char **signature) {
	char *s = dbus_message_iter_get_signature(msg->iter);
	if (s == NULL) {
		return -ENOMEM;
	}

	*signature = strdup(s);
	dbus_free(s);
	return 0;
}

int sd_bus_message_readv(sd_bus_message *msg, const char *signature, va_list ap) {
	// Immediately return 0 if there are not arguments to read.
	if (dbus_message_iter_get_arg_type(msg->iter) == DBUS_TYPE_INVALID) {
		return 0;
	}

	DBusSignatureIter iter;
	dbus_signature_iter_init(&iter, signature);
	do {
		// Bail out if the signature contains a non-basic type.
		int type = dbus_signature_iter_get_current_type(&iter);
		if (!dbus_type_is_basic(type)) {
			va_end(ap);
			return -EINVAL;
		}

		// Bail out if signature does not match the actual argument.
		if (dbus_message_iter_get_arg_type(msg->iter) != type) {
			return -ENXIO;
		}

		// Read the value.
		DBusBasicValue *ptr = va_arg(ap, DBusBasicValue *);
		if (ptr != NULL) {
			dbus_message_iter_get_basic(msg->iter, ptr);
		}

		// Don't check for "no more args" condition here, since it is possible
		// that there are no more elements in the signature either. The erronous
		// "signature not ended, but no more args" scenario is dealt with at the
		// beginning of the loop body by DBUS_TYPE_INVALID not being a basic
		// type.
		dbus_message_iter_next(msg->iter);
	} while (dbus_signature_iter_next(&iter));

	return 1;
}

int sd_bus_message_read(sd_bus_message *msg, const char *signature, ...) {
	int ret = 0;
	va_list ap;
	va_start(ap, signature);

	// NOTE: There is a difference here between the real sd_bus_message_read and
	// this wrapper. We only support messages that
	//  - contain a variant type that contain only basic types,
	//  - or contain only basic types.
	//
	// This is currently enough for Mako, but I left this note in case this
	// function needs to be expanded in the future.
	if (strcmp(signature, "v") == 0) {
		const char *inner_signature = va_arg(ap, char *);
		sd_bus_message_enter_container(msg, 'v', inner_signature);
		ret = sd_bus_message_readv(msg, inner_signature, ap);
		sd_bus_message_exit_container(msg);
	} else {
		ret = sd_bus_message_readv(msg, signature, ap);
	}

	va_end(ap);
	return ret;
}

int sd_bus_message_skip(sd_bus_message *msg, const char *signature) {
	// Immediately return 0 if there are not arguments to read.
	if (dbus_message_iter_get_arg_type(msg->iter) == DBUS_TYPE_INVALID) {
		return 0;
	}

	DBusSignatureIter iter;
	dbus_signature_iter_init(&iter, signature);
	do {
		// Bail out if the signature contains a non-basic type.
		int type = dbus_signature_iter_get_current_type(&iter);
		if (!dbus_type_is_basic(type)) {
			return -EINVAL;
		}

		// Bail out if signature does not match the actual argument.
		if (dbus_message_iter_get_arg_type(msg->iter) != type) {
			return -ENXIO;
		}

		// Don't check for "no more args" condition here, since it is possible
		// that there are no more elements in the signature either. The erronous
		// "signature not ended, but no more args" scenario is dealt with at the
		// beginning of the loop body by DBUS_TYPE_INVALID not being a basic
		// type.
		dbus_message_iter_next(msg->iter);
	} while (dbus_signature_iter_next(&iter));

	return dbus_message_iter_has_next(msg->iter);
}

int sd_bus_message_enter_container(sd_bus_message *msg, char type,
		const char *signature) {
	// Immediately return 0 if there are no arguments to read (or in this case,
	// no container to enter), or -ENXIO if signature and actual argument type
	// does not match.
	int t = dbus_message_iter_get_arg_type(msg->iter);
	if (t == DBUS_TYPE_INVALID) {
		return 0;
	} else if (t != type) {
		return -ENXIO;
	}

	// Allocate an iterator, recurse into it, and set it as the current iterator
	// of the message.
	struct msg_iter *sub_iter = malloc(sizeof(struct msg_iter));
	if (sub_iter == NULL) {
		return -ENOMEM;
	}

	dbus_message_iter_recurse(msg->iter, &sub_iter->iter);
	wl_list_insert(msg->iters, &sub_iter->link);
	msg->iter = &sub_iter->iter;

	return dbus_message_iter_has_next(&sub_iter->iter);
}

int sd_bus_message_exit_container(sd_bus_message *msg) {
	// Remove the iterator we want to exit from from the iterator stack
	struct msg_iter *iter = wl_container_of(msg->iters->next, iter, link);
	wl_list_remove(&iter->link);

	// Set msg's current iterator to the top of the iterator stack, and use
	// dbus_message_iter_next to "step over" the iterator we are exiting.
	iter = wl_container_of(msg->iters->next, iter, link);
	msg->iter = &iter->iter;
	dbus_message_iter_next(msg->iter);

	return 0;
}

int sd_bus_emit_signal(sd_bus *bus, const char *path, const char *interface,
		const char *name, const char *signature, ...) {
	int ret = 0;
	va_list ap;
	va_start(ap, signature);

	DBusMessage *signal = NULL;
	if ((signal = dbus_message_new_signal(path, interface, name)) == NULL) {
		ret = -ENOMEM;
		goto finish;
	}

	DBusSignatureIter signature_iter;
	DBusMessageIter message_iter;
	dbus_signature_iter_init(&signature_iter, signature);
	dbus_message_iter_init_append(signal, &message_iter);
	do {
		int type = dbus_signature_iter_get_current_type(&signature_iter);
		if (!dbus_type_is_basic(type)) {
			ret = -EINVAL;
			goto finish;
		}

		DBusBasicValue val = va_arg(ap, DBusBasicValue);
		if (!dbus_message_iter_append_basic(&message_iter, type, &val)) {
			ret = -ENOMEM;
			goto finish;
		}
	} while (dbus_signature_iter_next(&signature_iter));

	if (!dbus_connection_send(bus, signal, NULL)) {
		ret = -ENOMEM;
		goto finish;
	}

finish:
	va_end(ap);
	dbus_message_unref(signal);
	return ret;
}

int sd_bus_reply_method_return(sd_bus_message *msg, const char *signature, ...) {
	int ret = 0;
	va_list ap;
	va_start(ap, signature);

	DBusMessage *reply = NULL;
	if ((reply = dbus_message_new_method_return(msg->message)) == NULL) {
		ret = -ENOMEM;
		goto finish;
	}

	if (strlen(signature) == 0) {
		goto send;
	}

	DBusSignatureIter signature_iter;
	DBusMessageIter message_iter;
	dbus_signature_iter_init(&signature_iter, signature);
	dbus_message_iter_init_append(reply, &message_iter);
	do {
		int type = dbus_signature_iter_get_current_type(&signature_iter);
		if (!dbus_type_is_basic(type)) {
			ret = -EINVAL;
			goto finish;
		}

		DBusBasicValue val = va_arg(ap, DBusBasicValue);
		if (!dbus_message_iter_append_basic(&message_iter, type, &val)) {
			ret = -ENOMEM;
			goto finish;
		}
	} while (dbus_signature_iter_next(&signature_iter));

send:
	if (!dbus_connection_send(msg->bus, reply, NULL)) {
		ret = -ENOMEM;
		goto finish;
	}

finish:
	va_end(ap);
	dbus_message_unref(reply);
	return ret;
}

int sd_bus_send(sd_bus *bus, sd_bus_message *msg, dbus_uint32_t *cookie) {
	// NOTE: The original sd_bus_send takes an uint64_t * as the third parameter.
	// We take a dbus_uint32_t *, because dbus_connection_send uses it.

	sd_bus *conn = (bus == NULL ? msg->bus : bus);
	if (!dbus_connection_send(conn, msg->message, cookie)) {
		return -ENOMEM;
	}

	return 0;
}
