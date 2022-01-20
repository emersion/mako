#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <math.h>

#include "config.h"
#include "criteria.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "icon.h"

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

static void move_to(cairo_t *cairo, double x, double y, int scale) {
	cairo_move_to(cairo, x * scale, y * scale);
}

static void set_rounded_rectangle(cairo_t *cairo, double x, double y, double width, double height,
		int scale, int radius) {
	if (width == 0 || height == 0) {
		return;
	}
	x *= scale;
	y *= scale;
	width *= scale;
	height *= scale;
	radius *= scale;
	double degrees = M_PI / 180.0;

	cairo_new_sub_path(cairo);
	cairo_arc(cairo, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
	cairo_arc(cairo, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
	cairo_arc(cairo, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
	cairo_arc(cairo, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
	cairo_close_path(cairo);
}

static cairo_subpixel_order_t get_cairo_subpixel_order(
		enum wl_output_subpixel subpixel) {
	switch (subpixel) {
	case WL_OUTPUT_SUBPIXEL_UNKNOWN:
	case WL_OUTPUT_SUBPIXEL_NONE:
		return CAIRO_SUBPIXEL_ORDER_DEFAULT;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_RGB;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_BGR;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_VRGB;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_VBGR;
	}
	abort();
}

static void set_font_options(cairo_t *cairo, struct mako_surface *surface) {
	if (surface->surface_output == NULL) {
		return;
	}

	cairo_font_options_t *fo = cairo_font_options_create();
	if (surface->surface_output->subpixel == WL_OUTPUT_SUBPIXEL_NONE) {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
	} else {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
		cairo_font_options_set_subpixel_order(fo,
			get_cairo_subpixel_order(surface->surface_output->subpixel));
	}
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
}

static int render_notification(cairo_t *cairo, struct mako_state *state, struct mako_surface *surface,
		struct mako_style *style, const char *text, struct mako_icon *icon, int offset_y, uint32_t scale,
		struct mako_hotspot *hotspot, int progress) {
	int border_size = 2 * style->border_size;
	int padding_height = style->padding.top + style->padding.bottom;
	int padding_width = style->padding.left + style->padding.right;
	int radius = style->border_radius;
	bool icon_vertical = style->icon_location == MAKO_ICON_LOCATION_TOP ||
		style->icon_location == MAKO_ICON_LOCATION_BOTTOM;

	// If the compositor has forced us to shrink down, do so.
	int notif_width =
		(style->width <= surface->width) ? style->width : surface->width;

	// offset_x is for the entire draw operation inside the surface
	int offset_x;
	if (surface->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		offset_x = surface->width - notif_width - style->margin.right;
	} else if (surface->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		offset_x = style->margin.left;
	} else { // CENTER has nothing to & with, so it's the else case
		offset_x = (surface->width - notif_width) / 2;
	}

	// text_x is the offset of the text inside our draw operation
	double text_x = style->padding.left;
	if (icon != NULL && style->icon_location == MAKO_ICON_LOCATION_LEFT) {
		text_x = icon->width + 2*style->padding.left;
	}

	// text_y is the offset of the text inside our draw operation
	double text_y = style->padding.top;
	if (icon != NULL && style->icon_location == MAKO_ICON_LOCATION_TOP) {
		text_y = icon->height + 2*style->padding.top;
	}

	double text_layout_width = notif_width - border_size - padding_width;
	if (icon && ! icon_vertical) {
		text_layout_width -= icon->width;
		text_layout_width -= style->icon_location == MAKO_ICON_LOCATION_LEFT ?
			(style->padding.left * 2) : (style->padding.right * 2);
	}
	double text_layout_height = style->height - border_size - padding_height;
	if (icon && icon_vertical) {
		text_layout_height -= icon->height;
		text_layout_height -= style->icon_location == MAKO_ICON_LOCATION_TOP ?
			(style->padding.top * 2) : (style->padding.bottom * 2);
	}

	set_font_options(cairo, surface);

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	set_layout_size(layout, text_layout_width, text_layout_height, scale);
	pango_layout_set_alignment(layout, style->text_alignment);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	PangoFontDescription *desc =
		pango_font_description_from_string(style->font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	PangoAttrList *attrs = NULL;
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

	if (attrs == NULL) {
		attrs = pango_attr_list_new();
	}
	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);

	int buffer_text_height = 0;
	int buffer_text_width = 0;

	// If there's no text to be rendered, the notification can shrink down
	// smaller than the line height.
	if (pango_layout_get_character_count(layout) > 0) {
		pango_layout_get_pixel_size(layout, &buffer_text_width, &buffer_text_height);
	}
	int text_height = buffer_text_height / scale;
	int text_width = buffer_text_width / scale;

	if (text_height > text_layout_height) {
		text_height = text_layout_height;
	}

	int notif_height = text_height + border_size + padding_height;
	if (icon && icon_vertical) {
		notif_height += icon->height;
		notif_height += style->icon_location == MAKO_ICON_LOCATION_TOP ?
			style->padding.top : style->padding.bottom;
	}
	if (icon != NULL && ! icon_vertical && icon->height > text_height) {
		notif_height = icon->height + border_size + padding_height;
	}

	if (notif_height < radius * 2) {
		notif_height = radius * 2 + border_size;
	}

	int notif_background_width = notif_width - style->border_size;

	// Define the shape of the notification. The stroke is drawn centered on
	// the edge of the fill, so we need to inset the shape by half the
	// border_size.
	set_rounded_rectangle(cairo,
		offset_x + style->border_size / 2.0,
		offset_y + style->border_size / 2.0,
		notif_background_width,
		notif_height - style->border_size,
		scale, radius);

	// Render background, keeping the path.
	set_source_u32(cairo, style->colors.background);
	cairo_fill_preserve(cairo);

	// Keep a copy of the path. We need it later to draw the border on top, but
	// we have to create a new one for progress in the meantime.
	cairo_path_t *border_path = cairo_copy_path(cairo);

	// Render progress. We need to render this as a normal rectangle, but clip
	// it to the rounded rectangle we drew for the background. We also inset it
	// a bit further so that 0 and 100 percent are aligned to the inside edge
	// of the border and we can actually see the whole range.
	int progress_width =
		(notif_background_width - style->border_size) * progress / 100;
	if (progress_width < 0) {
		progress_width = 0;
	} else if (progress_width > notif_background_width) {
		progress_width = notif_background_width - style->border_size;
	}

	cairo_save(cairo);
	cairo_clip(cairo);
	cairo_set_operator(cairo, style->colors.progress.operator);
	set_source_u32(cairo, style->colors.progress.value);
	set_rounded_rectangle(cairo,
			offset_x + style->border_size,
			offset_y + style->border_size,
			progress_width,
			notif_height - style->border_size,
			scale, 0);
	cairo_fill(cairo);
	cairo_restore(cairo);

	// Render border, using the SOURCE operator to clip away the background
	// and progress beneath. This is the only way to make the background appear
	// to line up with the inside of a rounded border, while not revealing any
	// of the background when using a translucent border color.
	cairo_save(cairo);
	cairo_append_path(cairo, border_path);
	set_source_u32(cairo, style->colors.border);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_line_width(cairo, style->border_size * scale);
	cairo_stroke(cairo);
	cairo_restore(cairo);

	cairo_path_destroy(border_path);

	if (icon != NULL) {
		// Render icon
		double xpos = -1;
		double ypos = -1;
		double ypos_center = offset_y + style->border_size +
			(notif_height - icon->height - border_size) / 2;
		double xpos_center = offset_x + style->border_size +
			(notif_width - icon->width - border_size) / 2;

		switch (style->icon_location) {
		case MAKO_ICON_LOCATION_LEFT:
			xpos = offset_x + style->border_size +
				(text_x - icon->width) / 2;
			ypos = ypos_center;
			break;
		case MAKO_ICON_LOCATION_RIGHT:
			xpos = offset_x + notif_width - style->border_size -
				icon->width - style->margin.right;
			ypos = ypos_center;
			break;
		case MAKO_ICON_LOCATION_TOP:
			xpos = xpos_center;
			ypos = offset_y + style->border_size;
			break;
		case MAKO_ICON_LOCATION_BOTTOM:
			xpos = xpos_center;
			ypos = offset_y + text_y + text_height + style->margin.bottom;
			break;
		}
		draw_icon(cairo, icon, xpos, ypos, scale);
	}

	if (icon_vertical) {
		text_x = (notif_width - text_width) / 2;
	} else {
		text_y = (notif_height - text_height) / 2;
	}

	// Render text
	set_source_u32(cairo, style->colors.text);
	move_to(cairo,
		offset_x + style->border_size + text_x,
		offset_y + style->border_size + text_y,
		scale);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	// Update hotspot with calculated location
	if (hotspot != NULL) {
		hotspot->x = offset_x;
		hotspot->y = offset_y;
		hotspot->width = notif_width;
		hotspot->height = notif_height;
	}

	g_object_unref(layout);

	return notif_height;
}

int render(struct mako_surface *surface, struct pool_buffer *buffer, uint32_t scale) {
	struct mako_state *state = surface->state;
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

	size_t visible_count = 0;
	size_t hidden_count = 0;
	int total_height = 0;
	int pending_bottom_margin = 0;
	struct mako_notification *notif;
	size_t total_notifications = 0;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->surface != surface) {
			continue;
		}
		++total_notifications;

		// Immediately before rendering we need to re-match all of the criteria
		// so that matches against the anchor and output work even if the
		// output was automatically assigned by the compositor.
		int rematch_count = apply_each_criteria(&state->config.criteria, notif);
		if (rematch_count == -1) {
			// We encountered an allocation failure or similar while applying
			// criteria. The notification may be partially matched, but the
			// worst case is that it has an empty style, so bail.
			fprintf(stderr, "Failed to apply criteria\n");
			break;
		} else if (rematch_count == 0) {
			// This should be impossible, since the global criteria is always
			// present in a mako_config and matches everything.
			fprintf(stderr, "Notification matched zero criteria?!\n");
			break;
		}

		// Note that by this point, everything in the style is guaranteed to
		// be specified, so we don't need to check.
		struct mako_style *style = &notif->style;

		if (style->max_visible >= 0 &&
				visible_count >= (size_t)style->max_visible) {
			++hidden_count;
			continue;
		}

		if (style->invisible) {
			continue;
		}

		size_t text_len =
			format_text(style->format, NULL, format_notif_text, notif);

		char *text = malloc(text_len + 1);
		if (text == NULL) {
			fprintf(stderr, "Unable to allocate memory to render notification\n");
			break;
		}
		format_text(style->format, text, format_notif_text, notif);

		if (style->margin.top > pending_bottom_margin) {
			total_height += style->margin.top;
		} else {
			total_height += pending_bottom_margin;
		}

		struct mako_icon *icon = (style->icons) ? notif->icon : NULL;
		int notif_height = render_notification(
			cairo, state, surface, style, text, icon, total_height, scale,
			&notif->hotspot, notif->progress);
		free(text);

		total_height += notif_height;
		pending_bottom_margin = style->margin.bottom;

		if (notif->group_index < 1) {
			// If the notification is ungrouped, or is the first in a group, it
			// counts against max_visible. Even if other notifications in the
			// group are rendered based on criteria, a group is considered a
			// single entity for this purpose.
			++visible_count;
		}

	}

	if (hidden_count > 0) {
		struct mako_notification *hidden_notif = create_notification(state);
		hidden_notif->surface = surface;
		hidden_notif->hidden = true;
		apply_each_criteria(&state->config.criteria, hidden_notif);

		struct mako_style *style = &hidden_notif->style;

		if (style->margin.top > pending_bottom_margin) {
			total_height += style->margin.top;
		} else {
			total_height += pending_bottom_margin;
		}

		struct mako_hidden_format_data data = {
			.hidden = hidden_count,
			.count = total_notifications,
		};

		size_t text_ln =
			format_text(style->format, NULL, format_hidden_text, &data);
		char *text = malloc(text_ln + 1);
		if (text == NULL) {
			fprintf(stderr, "allocation failed");
			return 0;
		}

		format_text(style->format, text, format_hidden_text, &data);

		int hidden_height = render_notification(
			cairo, state, surface, style, text, NULL, total_height, scale, NULL, 0);
		free(text);
		destroy_notification(hidden_notif);

		total_height += hidden_height;
		pending_bottom_margin = style->margin.bottom;
	}

	return total_height + pending_bottom_margin;
}
