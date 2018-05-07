#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "notification.h"
#include "render.h"

static void set_cairo_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
		(color >> (3*8) & 0xFF) / 255.0,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0);
}

void render(struct mako_state *state) {
	struct mako_config *config = &state->config;

	if (wl_list_empty(&state->notifications)) {
		// Unmap the surface
		wl_surface_attach(state->surface, NULL, 0, 0);
		wl_surface_commit(state->surface);
		return;
	}

	state->current_buffer = get_next_buffer(state->shm, state->buffers,
		state->width, state->height);

	cairo_t *cairo = state->current_buffer->cairo;
	set_cairo_source_u32(cairo, config->colors.background);
	cairo_paint(cairo);

	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		PangoLayout *layout = pango_cairo_create_layout(cairo);
		pango_layout_set_width(layout,
			(state->width - 2 * config->padding) * PANGO_SCALE);
		pango_layout_set_height(layout,
			(state->height - 2 * config->padding) * PANGO_SCALE);
		pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
		PangoFontDescription *desc =
			pango_font_description_from_string(config->font);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);

		char *text = format_notification(notif, config->format);
		if (text == NULL) {
			return;
		}

		if (config->markup) {
			PangoAttrList *attrs = NULL;
			char *buf = NULL;
			GError *error = NULL;
			if (!pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
				fprintf(stderr, "cannot parse pango markup: %s\n",
					error->message);
				return; // TODO: better error handling
			}
			pango_layout_set_markup(layout, buf, -1);
			pango_layout_set_attributes(layout, attrs);
			pango_attr_list_unref(attrs);
			free(buf);
		} else {
			pango_layout_set_text(layout, text, -1);
		}
		free(text);

		set_cairo_source_u32(cairo, config->colors.text);
		cairo_move_to(cairo, config->padding, config->padding);
		pango_cairo_update_layout(cairo, layout);
		pango_cairo_show_layout(cairo, layout);

		g_object_unref(layout);

		break; // TODO: support multiple notifications
	}

	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}
