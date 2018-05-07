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

int render(struct mako_state *state, struct pool_buffer *buffer) {
	struct mako_config *config = &state->config;

	if (wl_list_empty(&state->notifications)) {
		return 0;
	}

	cairo_t *cairo = buffer->cairo;
	set_cairo_source_u32(cairo, config->colors.background);
	cairo_paint(cairo);

	int height = 0;
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		int padding_size = 2 * config->padding;

		PangoLayout *layout = pango_cairo_create_layout(cairo);
		pango_layout_set_width(layout,
			(config->width - padding_size) * PANGO_SCALE);
		pango_layout_set_height(layout,
			(config->height - padding_size) * PANGO_SCALE);
		pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
		PangoFontDescription *desc =
			pango_font_description_from_string(config->font);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);

		char *text = format_notification(notif, config->format);
		if (text == NULL) {
			break;
		}

		if (config->markup) {
			PangoAttrList *attrs = NULL;
			char *buf = NULL;
			GError *error = NULL;
			if (!pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
				fprintf(stderr, "cannot parse pango markup: %s\n",
					error->message);
				g_error_free(error);
				break; // TODO: better error handling
			}
			pango_layout_set_markup(layout, buf, -1);
			pango_layout_set_attributes(layout, attrs);
			pango_attr_list_unref(attrs);
			free(buf);
		} else {
			pango_layout_set_text(layout, text, -1);
		}
		free(text);

		int text_height = 0;
		pango_layout_get_pixel_size(layout, NULL, &text_height);
		height = text_height + padding_size;

		set_cairo_source_u32(cairo, config->colors.text);
		cairo_move_to(cairo, config->padding, config->padding);
		pango_cairo_update_layout(cairo, layout);
		pango_cairo_show_layout(cairo, layout);

		g_object_unref(layout);

		break; // TODO: support multiple notifications
	}

	return height;
}
