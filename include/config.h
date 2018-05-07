#ifndef _MAKO_CONFIG_H
#define _MAKO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

struct mako_config {
	char *font;
	int32_t width, height;
	int32_t margin, padding;
	int32_t border_size;
	bool markup;
	char *format;
	bool actions;

	struct {
		uint32_t background;
		uint32_t text;
		uint32_t border;
	} colors;
};

void init_config(struct mako_config *config);
void finish_config(struct mako_config *config);
int parse_config_arguments(struct mako_config *config, int argc, char **argv);

#endif
