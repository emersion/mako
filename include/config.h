#ifndef _MAKO_CONFIG_H
#define _MAKO_CONFIG_H

#include <stdint.h>

struct mako_config {
	char *font;
	int32_t margin, padding;

	struct {
		uint32_t background;
		uint32_t text;
	} colors;
};

void parse_config_arguments(struct mako_config *config, int argc, char **argv);
void finish_config(struct mako_config *config);

#endif
