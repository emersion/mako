#ifndef _SUBD_H
#define _SUBD_H

#include <semaphore.h>
#include "subd-sdbus.h"

struct pollfd;

/**
 * This struct stores the DBus watches, and their corresponging file
 * descriptors. This will be auto-updated whenever DBus needs additional file
 * descriptors to watch. You can also store other, not DBus-related file
 * descriptors here, so you can directly use fds as a parameter for poll().
 * If you add non-watch file descriptors, make sure to set their corresponding
 * watch to NULL, so that #subd_process_watches knows to skip them.
 */
struct subd_watch_store {
	struct pollfd *fds;
	DBusWatch **watches;
	int capacity;	
	int length;
	sem_t mutex;
};

/**
 * This function creates a subd_watches instance, and pre-populates it with
 * the non-DBus file descriptors passed as fds. It also registers the add,
 * remove and toggle functions that will handle automatic file descriptor
 * additions/removals.
 */
struct subd_watch_store *subd_init_watches(DBusConnection *conn,
	struct pollfd *fds, int size, DBusError *err);

/**
 * This function should be called from the event loop after a successful poll to
 * handle the DBus watches that need to be handled (= the watches whose file
 * descriptor returned an event).
 */
void subd_process_watches(DBusConnection *conn, struct subd_watch_store *watch_store);

#endif
