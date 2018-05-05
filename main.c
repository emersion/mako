#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "main.h"

struct mako_state mako = { 0 };

int main(int argc, char *argv[]) {
	sd_bus *bus = NULL;
	sd_bus_slot *slot = NULL;
	if (!init_dbus(&bus, &slot)) {
		return EXIT_FAILURE;
	}

	int ret = 0;
	for (;;) {
		/* Process requests */
		ret = sd_bus_process(bus, NULL);
		if (ret < 0) {
			fprintf(stderr, "Failed to process bus: %s\n", strerror(-ret));
			goto finish;
		}
		if (ret > 0) {
			// We processed a request, try to process another one, right-away
			continue;
		}

		// Wait for the next request to process
		ret = sd_bus_wait(bus, (uint64_t)-1);
		if (ret < 0) {
			fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-ret));
			goto finish;
		}
	}

finish:
	sd_bus_slot_unref(slot);
	sd_bus_unref(bus);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
