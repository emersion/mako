#include <errno.h>
#include <poll.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include "subd.h"

static dbus_bool_t add_watch(DBusWatch *watch, void *data) {
	struct subd_watch_store *watch_store = data;
	sem_wait(&watch_store->mutex);

	if (!dbus_watch_get_enabled(watch)) {
		sem_post(&watch_store->mutex);
		return TRUE;
	}

	if (watch_store->length == watch_store->capacity) {
		int c = watch_store->capacity + 10;

		void *new_fds = realloc(watch_store->fds, sizeof(struct pollfd) * c);
		if (new_fds != NULL) {
			watch_store->fds = new_fds;
		} else {
			sem_post(&watch_store->mutex);
			return FALSE;
		}

		void *new_watches = realloc(watch_store->watches, sizeof(DBusWatch *) * c);
		if (new_watches != NULL) {
			watch_store->watches = new_watches;
		} else {
			sem_post(&watch_store->mutex);
			return FALSE;
		}

		watch_store->capacity = c;
	}

	short mask = 0;
	unsigned int flags = dbus_watch_get_flags(watch);
	if (flags & DBUS_WATCH_READABLE) {
		mask |= POLLIN;
	}
	if (flags & DBUS_WATCH_WRITABLE) {
		mask |= POLLOUT;
	}

	int fd = dbus_watch_get_unix_fd(watch);
	struct pollfd pollfd = {fd, mask, 0};
	int index = watch_store->length++;
	watch_store->fds[index] = pollfd;
	watch_store->watches[index] = watch;

	sem_post(&watch_store->mutex);
	return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data) {
	struct subd_watch_store *watch_store = data;
	sem_wait(&watch_store->mutex);

	int index = -1;
	for (int i = 0; i < watch_store->length; ++i) {
		if (watch_store->watches[i] == watch) {
			index = i;
			break;
		}
	}

	if (index != -1) {
		--watch_store->length;
		memmove(&watch_store->fds[index], &watch_store->fds[index + 1],
			sizeof(struct pollfd) * watch_store->length - index);
		memmove(&watch_store->watches[index], &watch_store->watches[index + 1],
			sizeof(DBusWatch *) * watch_store->length - index);
	}

	sem_post(&watch_store->mutex);
}

static void toggle_watch(DBusWatch *watch, void *data) {
	if (dbus_watch_get_enabled(watch)) {
		add_watch(watch, data);
	} else {
		remove_watch(watch, data);
	}
}

struct subd_watch_store *subd_init_watches(DBusConnection *conn,
		struct pollfd *fds, int size, DBusError *err) {
	const char *error_code = NULL;

	// Initialize the watches structure.
	struct subd_watch_store *watch_store = malloc(sizeof(struct subd_watch_store));
	if (watch_store == NULL) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	watch_store->capacity = 10 + size;
	watch_store->length = size;
	watch_store->fds = calloc(watch_store->capacity, sizeof(struct pollfd));
	watch_store->watches = calloc(watch_store->capacity, sizeof(DBusWatch*));
	if (watch_store->fds == NULL || watch_store->watches == NULL) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	// Initialize a semaphore for the watches. It is needed to prevent the add,
	// remove and toggle functions accessing the watch storage while it is being
	// processed by subd_process_watches.
	if (sem_init(&watch_store->mutex, 0, 1) == -1) {
		if (errno == EINVAL) {
			error_code = DBUS_ERROR_INVALID_ARGS;
		} else {
			error_code = DBUS_ERROR_NO_MEMORY;
		}
		goto error;
	}

	// Add any non-dbus file descriptors.
	for (int i = 0; i < size; ++i) {
		watch_store->fds[i] = (struct pollfd){
			.fd = fds->fd,
			.events = fds->events
		};
		watch_store->watches[i] = NULL;
		++fds;
	}

	// Register the add, remove, and toggle functions.
	// NOTE: Can't use the free_data_function argument to automatically free
	// watches when connection finalizes, because sometimes it does not work.
	if (!dbus_connection_set_watch_functions(conn, add_watch, remove_watch,
			toggle_watch, watch_store, NULL)) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	return watch_store;

error:
	if (watch_store != NULL) {
		free(watch_store->fds);
		free(watch_store->watches);
		free(watch_store);
	}
	dbus_set_error(err, error_code, NULL);
	return NULL;
}

void subd_process_watches(DBusConnection *conn, struct subd_watch_store *watch_store) {
	sem_wait(&watch_store->mutex);

	for (int i = 0; i < watch_store->length; ++i) {
		struct pollfd pollfd = watch_store->fds[i];
		DBusWatch *watch = watch_store->watches[i];

		if (watch == NULL || !dbus_watch_get_enabled(watch)) {
			continue;
		}

		if (pollfd.revents & pollfd.events) {
			unsigned int flags = 0;
			if (pollfd.revents & POLLIN) {
				flags |= DBUS_WATCH_READABLE;
			}
			if (pollfd.revents & POLLOUT) {
				flags |= DBUS_WATCH_WRITABLE;
			}
			if (pollfd.revents & POLLHUP) {
				flags |= DBUS_WATCH_HANGUP;
			}
			if (pollfd.revents & POLLERR) {
				flags |= DBUS_WATCH_ERROR;
			}

			dbus_watch_handle(watch, flags);

			while (dbus_connection_dispatch(conn) ==
					DBUS_DISPATCH_DATA_REMAINS);
		}
	}

	sem_post(&watch_store->mutex);
}
