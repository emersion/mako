#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"

// HiDPI conventions: local variables are in surface-local coordinates, unless
// they have a "buffer_" prefix, in which case they are in buffer-local
// coordinates.

static void set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
		(color >> (3*8) & 0xFF) / 255.0,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0);
}

static void set_layout_size(PangoLayout *layout, int width, int height,
		int scale) {
	pango_layout_set_width(layout, width * scale * PANGO_SCALE);
	pango_layout_set_height(layout, height * scale * PANGO_SCALE);
}

static void move_to(cairo_t *cairo, int x, int y, int scale) {
	cairo_move_to(cairo, x * scale, y * scale);
}

static void set_rectangle(cairo_t *cairo, int x, int y, int width, int height,
		int scale) {
	cairo_rectangle(cairo, x * scale, y * scale, width * scale, height * scale);
}

static int render_notification(cairo_t *cairo, struct mako_state *state,
		const char *text, int offset_y, int scale) {
	struct mako_config *config = &state->config;

	int border_size = 2 * config->border_size;
	int padding_size = 2 * config->padding;

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	set_layout_size(layout,
		config->width - border_size - padding_size,
		config->height - border_size - padding_size, scale);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	PangoFontDescription *desc =
		pango_font_description_from_string(config->font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	PangoAttrList *attrs = NULL;
	if (config->markup) {
		GError *error = NULL;
		char *buf = NULL;
		if (pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
			pango_layout_set_text(layout, buf, -1);
			free(buf);
		} else {
			fprintf(stderr, "cannot parse pango markup: %s\n", error->message);
			g_error_free(error);
			// fallback to plain text
			pango_layout_set_text(layout, text, -1);
		}
	} else {
		pango_layout_set_text(layout, text, -1);
	}

	if (attrs == NULL) {
		attrs = pango_attr_list_new();
	}
	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);

	int buffer_text_height = 0;
	pango_layout_get_pixel_size(layout, NULL, &buffer_text_height);
	int text_height = buffer_text_height / scale;

	int notif_height = border_size + padding_size + text_height;

	// Render border
	set_source_u32(cairo, config->colors.border);
	set_rectangle(cairo,
		config->border_size / 2.0,
		offset_y + config->border_size / 2.0,
		config->width - config->border_size,
		notif_height - config->border_size, scale);
	cairo_save(cairo);
	cairo_set_line_width(cairo, config->border_size * scale);
	cairo_stroke(cairo);
	cairo_restore(cairo);

	// Render background
	set_source_u32(cairo, config->colors.background);
	set_rectangle(cairo,
		config->border_size, offset_y + config->border_size,
		config->width - border_size, notif_height - border_size, scale);
	cairo_fill(cairo);

	// Render text
	set_source_u32(cairo, config->colors.text);
	move_to(cairo, config->border_size + config->padding,
		offset_y + config->border_size + config->padding, scale);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	g_object_unref(layout);

	return notif_height;
}

int render(struct mako_state *state, struct pool_buffer *buffer, int scale) {
	struct mako_config *config = &state->config;
	cairo_t *cairo = buffer->cairo;

	if (wl_list_empty(&state->notifications)) {
		return 0;
	}

	// Clear
	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	int inner_margin = config->margin.top;
	if (config->margin.bottom > config->margin.top) {
		inner_margin = config->margin.bottom;
	}

	int notif_width = state->width;

	size_t i = 0;
	int total_height = 0;
	struct mako_notification *notif;
	wl_list_for_each(notif, &state->notifications, link) {
		size_t text_len =
			format_text(config->format, NULL, format_notif_text, notif);
		char *text = malloc(text_len + 1);
		if (text == NULL) {
			break;
		}
		format_text(config->format, text, format_notif_text, notif);

		if (i > 0) {
			total_height += inner_margin;
		}

		int notif_height =
			render_notification(cairo, state, text, total_height, scale);
		free(text);

		// Update hotspot
		notif->hotspot.x = 0;
		notif->hotspot.y = total_height;
		notif->hotspot.width = notif_width;
		notif->hotspot.height = notif_height;

		total_height += notif_height;

		++i;
		if (config->max_visible >= 0 &&
				i >= (size_t)config->max_visible) {
			break;
		}
	}

	if (wl_list_length(&state->notifications) > config->max_visible) {
		total_height += inner_margin;

		size_t text_ln =
			format_text(config->hidden_format, NULL, format_state_text, state);
		char *text = malloc(text_ln + 1);
		if (text == NULL) {
			fprintf(stderr, "allocation failed");
			return 0;
		}
		format_text(config->hidden_format, text, format_state_text, state);

		int hidden_height =
			render_notification(cairo, state, text, total_height, scale);
		free(text);

		total_height += hidden_height;
	}

	return total_height;
}
