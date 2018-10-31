#include <errno.h>
#include <poll.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include "subd.h"

static dbus_bool_t add_watch(DBusWatch *watch, void *data) {
	struct subd_watches *watches = data;
	sem_wait(&watches->mutex);

	if (!dbus_watch_get_enabled(watch)) {
		sem_post(&watches->mutex);
		return TRUE;
	}

	if (watches->length == watches->capacity) {
		int c = watches->capacity + 10;

		void *new_fds = realloc(watches->fds, sizeof(struct pollfd) * c);
		if (new_fds != NULL) {
			watches->fds = new_fds;
		} else {
			sem_post(&watches->mutex);
			return FALSE;
		}

		void *new_watches = realloc(watches->watches, sizeof(DBusWatch *) * c);
		if (new_watches != NULL) {
			watches->watches = new_watches;
		} else {
			sem_post(&watches->mutex);
			return FALSE;
		}

		watches->capacity = c;
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
	int index = watches->length++;
	watches->fds[index] = pollfd;
	watches->watches[index] = watch;

	sem_post(&watches->mutex);
	return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data) {
	struct subd_watches *watches = data;
	sem_wait(&watches->mutex);

	int index = -1;
	for (int i = 0; i < watches->length; ++i) {
		if (watches->watches[i] == watch) {
			index = i;
			break;
		}
	}

	if (index != -1) {
		--watches->length;
		memmove(&watches->fds[index], &watches->fds[index + 1],
			sizeof(struct pollfd) * watches->length - index);
		memmove(&watches->watches[index], &watches->watches[index + 1],
			sizeof(DBusWatch *) * watches->length - index);
	}

	sem_post(&watches->mutex);
}

static void toggle_watch(DBusWatch *watch, void *data) {
	if (dbus_watch_get_enabled(watch)) {
		add_watch(watch, data);
	} else {
		remove_watch(watch, data);
	}
}

struct subd_watches *subd_init_watches(struct DBusConnection *connection,
		struct pollfd *fds, int size, DBusError *error) {
	const char *error_code = NULL;

	// Initialize the watches structure.
	struct subd_watches *watches = malloc(sizeof(struct subd_watches));
	if (watches == NULL) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	watches->capacity = 10 + size;
	watches->length = size;
	watches->fds = calloc(watches->capacity, sizeof(struct pollfd));
	watches->watches = calloc(watches->capacity, sizeof(DBusWatch*));
	if (watches->fds == NULL || watches->watches == NULL) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	// Initialize a semaphore for the watches. It is needed to prevent the add,
	// remove and toggle functions accessing the watch storage while it is being
	// processed by subd_process_watches.
	if (sem_init(&watches->mutex, 0, 1) == -1) {
		if (errno == EINVAL) {
			error_code = DBUS_ERROR_INVALID_ARGS;
		} else {
			error_code = DBUS_ERROR_NO_MEMORY;
		}
		goto error;
	}

	// Add any non-dbus file descriptors.
	for (int i = 0; i < size; ++i) {
		watches->fds[i] = (struct pollfd){
			.fd = fds->fd,
			.events = fds->events
		};
		watches->watches[i] = NULL;
		++fds;
	}

	// Register the add, remove, and toggle functions.
	// NOTE: Can't use the free_data_function argument to automatically free
	// watches when connection finalizes, because sometimes it does not work.
	if (!dbus_connection_set_watch_functions(connection, add_watch,
			remove_watch, toggle_watch, watches, NULL)) {
		error_code = DBUS_ERROR_NO_MEMORY;
		goto error;
	}

	return watches;

error:
	if (watches != NULL) {
		free(watches->fds);
		free(watches->watches);
		free(watches);
	}
	dbus_set_error(error, error_code, NULL);
	return NULL;
}

void subd_process_watches(DBusConnection *conn, struct subd_watches *watches) {
	sem_wait(&watches->mutex);

	for (int i = 0; i < watches->length; ++i) {
		struct pollfd pollfd = watches->fds[i];
		DBusWatch *watch = watches->watches[i];

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

	sem_post(&watches->mutex);
}
