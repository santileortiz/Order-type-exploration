#if !defined(GUI_H)
#include <cairo/cairo.h>
#include <math.h>
#include "common.h"

void rounded_box_path (cairo_t *cr, double x, double y, double width, double height, double radius)
{
    cairo_move_to (cr, x, y+radius);
    cairo_arc (cr, x+radius, y+radius, radius, M_PI, 3*M_PI/2);
    cairo_arc (cr, x+width - radius, y+radius, radius, 3*M_PI/2, 0);
    cairo_arc (cr, x+width - radius, y+height - radius, radius, 0, M_PI/2);
    cairo_arc (cr, x+radius, y+height - radius, radius, M_PI/2, M_PI);
    cairo_close_path (cr);
}

bool is_point_in_box (double p_x, double p_y, double x, double y, double width, double height)
{
    if (p_x < x || p_x > x+width) {
        return false;
    } else if (p_y < y || p_y > y+height) {
        return false;
    } else {
        return true;
    }
}

void draw_button (cairo_t *cr, double x, double y, char *label, double *width, double *height)
{
    double border_radius = 2.5;
    double border_width = 1;
    double padding_x = 12;
    double padding_y = 3;

    cairo_text_extents_t extents;
    cairo_text_extents (cr, label, &extents);
    *width = extents.width + 2*(padding_x + border_width);
    *height = MAX (20, extents.height + 2*(padding_y)) + 2*border_width;

    cairo_save (cr);
    cairo_translate (cr, x, y);

    rounded_box_path (cr, 0, 0, *width+2*border_width,
                      *height+2*border_width, border_radius);
    cairo_clip (cr);

    cairo_set_source_rgb (cr, 0.96, 0.96, 0.96);
    cairo_paint (cr);

    cairo_pattern_t *patt = cairo_pattern_create_linear (border_width, border_width, 0, *height);
    cairo_pattern_add_color_stop_rgba (patt, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (patt, 0.5, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (patt, 1, 0, 0, 0, 0.04);
    cairo_set_source (cr, patt);
    cairo_paint (cr);

    rounded_box_path (cr, border_width/2, border_width/2,
                      *width+border_width, *height+border_width,
                      border_radius-border_width/2);
    cairo_set_source_rgba (cr, 0, 0, 0, 0.25);
    cairo_set_line_width (cr, border_width);
    cairo_stroke (cr);

    cairo_move_to (cr, (*width - extents.width)/2 + extents.x_bearing,
                   (*height - extents.height)/2 - extents.y_bearing);
    cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
    cairo_show_text (cr, label);

    cairo_restore (cr);
}

void draw_pressed_button (cairo_t *cr, double x, double y, char *label)
{
    double border_radius = 2.5;
    double border_width = 1;
    double padding_x = 12;
    double padding_y = 3;

    cairo_text_extents_t extents;
    cairo_text_extents (cr, label, &extents);
    double width = extents.width + 2*(padding_x + border_width);
    double height = MAX (20, extents.height + 2*(padding_y)) + 2*border_width;

    cairo_save (cr);
    cairo_translate (cr, x, y);

    rounded_box_path (cr, 0, 0, width+2*border_width,
                      height+2*border_width, border_radius);
    cairo_clip (cr);

    cairo_set_source_rgb (cr, 0.96, 0.96, 0.96);
    cairo_paint (cr);

    cairo_set_source_rgba (cr, 0, 0, 0, 0.05);
    cairo_paint (cr);

    rounded_box_path (cr, border_width/2, border_width/2,
                      width+border_width, height+border_width,
                      border_radius-border_width/2);
    cairo_set_source_rgba (cr, 0, 0, 0, 0.27);
    cairo_set_line_width (cr, border_width);
    cairo_stroke (cr);

    cairo_move_to (cr, (width - extents.width)/2 + extents.x_bearing,
                   (height - extents.height)/2 - extents.y_bearing);
    cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
    cairo_show_text (cr, label);

    cairo_restore (cr);
}
#define GUI_H
#endif
