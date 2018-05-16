#ifndef _MAKO_RENDER_H
#define _MAKO_RENDER_H

struct mako_state;

enum mako_render_text_alignment {
	MAKO_RENDER_TEXT_ALIGN_RIGHT = 1,
	MAKO_RENDER_TEXT_ALIGN_LEFT = 2,
	MAKO_RENDER_TEXT_ALIGN_CENTER = 3 
};

void cairo_clear(cairo_t *cairo, int x, int y, int width, int height);
PangoLayout *get_large_text_layout(cairo_t *cairo, struct mako_config *config, int max_width, int max_height, char *text);
PangoLayout *get_simple_text_layout(cairo_t *cairo, char *font, int max_width, int max_height);
void pango_render_layout(cairo_t *cairo, PangoLayout *layout, struct mako_config *config, int offset, uint32_t alignment);
void set_pango_font(PangoLayout *layout, char *font);
void set_pango_text(PangoLayout *layout, bool markup, char *text);
int render(struct mako_state *state, struct pool_buffer *buffer);

#endif
