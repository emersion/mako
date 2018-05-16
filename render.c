#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "notification.h"
#include "render.h"



void set_cairo_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
		(color >> (3*8) & 0xFF) / 255.0,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0);
}

//Clears the defined area if dimensions are set and the whole buffer if not.
void cairo_clear(cairo_t *cairo, int x, int y, int width, int height) {
	if (cairo == NULL) {
		return;
	}
	cairo_operator_t op = cairo_get_operator(cairo);

	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	set_cairo_source_u32(cairo, 0x00000000);
	if (x && y && width && height) {
		cairo_rectangle(cairo, x, y, width, height);
		cairo_fill(cairo);
	} else {
		cairo_paint(cairo);
	}

	cairo_set_operator(cairo, op);
}

PangoLayout *get_large_text_layout(cairo_t *cairo, struct mako_config *config, int max_width, int max_height, char *text) {
	if (cairo == NULL) {
		return NULL;
	}
	PangoLayout *layout = get_simple_text_layout(cairo, config->font, max_width, max_height);	
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	
	set_pango_text(layout, config->markup, text);

	return layout;
}

PangoLayout *get_simple_text_layout(cairo_t *cairo, char *font, int max_width, int max_height) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	if (max_width) {
		pango_layout_set_width(layout, max_width);
	}

	if (max_height) {
		pango_layout_set_height(layout, max_height);
	}
	
	set_pango_font(layout, font);

	return layout;
}

//renders text with common offsets (borders/padding). Perhaps this should include x_offset and y_offset?
void pango_render_layout(cairo_t *cairo, PangoLayout *layout, struct mako_config *config, int offset, uint32_t alignment) {
	if (cairo == NULL || layout == NULL) {
		return;
	}
	
	//Maybe we don't want to do this here, to allow different text colors for different areas?
	set_cairo_source_u32(cairo, config->colors.text);
	
	int text_width = 0, text_height = 0;
	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	int x = 0, y = offset + config->border_size;
	switch (alignment) {
	case MAKO_RENDER_TEXT_ALIGN_RIGHT:
		x = config->width - config->padding - config->border_size - text_width;
		break;

	case MAKO_RENDER_TEXT_ALIGN_LEFT:
		x = config->padding + config->border_size;
		break;

	case MAKO_RENDER_TEXT_ALIGN_CENTER:
		x = (config->width / 2) - (text_width / 2);
	}

	cairo_move_to(cairo, x, y);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	g_object_unref(layout);
}

void set_pango_font(PangoLayout *layout, char *font) {
	if (layout == NULL) {
		return;
	}
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
}

void set_pango_text(PangoLayout *layout, bool markup, char *text) {
	if (layout == NULL) {
		return;
	}

	if (markup) {
		PangoAttrList *attrs = NULL;
		char *buf = NULL;
		GError *error = NULL;
		if (!pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
			fprintf(stderr, "cannot parse pango markup: %s\n",
					error->message);
			g_error_free(error);
			free(text);
			return;
		}
		pango_layout_set_markup(layout, buf, -1);
		pango_layout_set_attributes(layout, attrs);
		pango_attr_list_unref(attrs);
		free(buf);
	} else {
		pango_layout_set_text(layout, text, -1);
	}
	free(text);
}

int render(struct mako_state *state, struct pool_buffer *buffer) {
	struct mako_config *config = &state->config;
	cairo_t *cairo = buffer->cairo;

	if (wl_list_empty(&state->notifications)) {
		return 0;
	}

	// Clear
	cairo_clear(cairo, 0, 0, 0, 0);

	int border_size = 2 * config->border_size;
	int padding_size = 2 * config->padding;

	int inner_margin = config->margin.top;
	if (config->margin.bottom > config->margin.top) {
		inner_margin = config->margin.bottom;
	}

	int notif_width = state->width;

	size_t i = 0;
	int height = 0;
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {

		size_t text_len = format_notification(notif, config->format, NULL);
		char *text = malloc(text_len + 1);
		if (text == NULL) {
			break;
		}
		format_notification(notif, config->format, text);
		
		PangoLayout *layout = get_large_text_layout(cairo, config, 
								  (config->width - border_size - padding_size) * PANGO_SCALE, 
								  (config->height -border_size - padding_size) * PANGO_SCALE, text);
		int text_height = 0;
		pango_layout_get_pixel_size(layout, NULL, &text_height);
		int notif_height = border_size + padding_size + text_height;

		if (i > 0) {
			height += inner_margin;
		}

		int notif_y = height;
		height += notif_height;

		// Render border
		set_cairo_source_u32(cairo, config->colors.border);
		cairo_set_line_width(cairo, border_size);
		cairo_rectangle(cairo, 0, notif_y, notif_width, notif_height);
		cairo_stroke(cairo);

		// Render background
		set_cairo_source_u32(cairo, config->colors.background);
		cairo_set_line_width(cairo, 0);
		cairo_rectangle(cairo,
				config->border_size, notif_y + config->border_size,
				notif_width - border_size, notif_height - border_size);
		cairo_fill(cairo);

		//Render Text
		pango_render_layout(cairo, layout, config, notif_y + config->padding, MAKO_RENDER_TEXT_ALIGN_RIGHT);

		// Update hotspot
		notif->hotspot.x = 0;
		notif->hotspot.y = notif_y;
		notif->hotspot.width = notif_width;
		notif->hotspot.height = notif_height;

		++i;
		if (i == (size_t)config->max_visible &&
				i < (size_t)wl_list_length(&state->notifications)) {

			int hidden = wl_list_length(&state->notifications) - i;
			int hidden_ln = snprintf(NULL, 0, "[%d]", hidden);

			char *hidden_text;
			hidden_text = malloc(hidden_ln + 1);
			if (hidden_text == NULL) {
				break;
			}

			snprintf(hidden_text, hidden_ln + 1, "[%d]", hidden);
			PangoLayout *layout = get_simple_text_layout(cairo, config->font, config->width, 0);

			int text_height = 0;
			pango_layout_get_pixel_size(layout, NULL, &text_height);
			set_pango_text(layout, false, hidden_text);
			pango_render_layout(cairo, layout, config, height, MAKO_RENDER_TEXT_ALIGN_CENTER);
			height += text_height;
		}

		if (config->max_visible >= 0 &&
				i >= (size_t)config->max_visible) {
			break;
		}
	}

	return height;
}
