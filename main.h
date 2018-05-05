#include <stdbool.h>
#include <systemd/sd-bus.h>

struct mako_state {
	uint32_t last_id;
};

struct mako_state mako;

bool init_dbus(sd_bus **bus, sd_bus_slot **slot);
