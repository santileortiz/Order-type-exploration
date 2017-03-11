#if !defined(GUI_H)
#include <cairo/cairo.h>
#include <math.h>
#include "common.h"

#define RGBA VECT4
#define RGB(r,g,b) VECT4(r,g,b,1)

int _g_gui_num_colors = 0;
vect4_t _g_gui_colors[50];

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

typedef struct {
    double border_radius;
    vect4_t border_color;
    double border_width;
    double padding_x;
    double padding_y;
    double width;
    double height;
    vect4_t background_color;
    vect4_t color;

    int num_gradient_stops;
    vect4_t *gradient_stops;

    vect2_t content_position;
    // TODO: ATM only text content is supported.
    char *label;
} css_box_t;

void css_box_set_fixed_size (css_box_t *box, double width, double height)
{
    box->width = width + 2*(box->padding_x + box->border_width);
    box->height = MAX (20, height) + 2*box->border_width;
}

// TODO: This function aligns text to the left but the others center it, add
// an align argument to specify what we want.
void css_box_compute_content_position (cairo_t *cr, css_box_t *box, char *label)
{
    cairo_text_extents_t extents;
    cairo_text_extents (cr, label, &extents);
    box->content_position.x = box->padding_x + extents.x_bearing;
    box->content_position.y = (box->height - extents.height)/2 - extents.y_bearing;
}

void css_box_compute_content_width_and_position (cairo_t *cr, css_box_t *box, char *label)
{
    cairo_text_extents_t extents;
    cairo_text_extents (cr, label, &extents);
    if (box->width == 0) {
        box->width = extents.width + 2*(box->padding_x + box->border_width);
    }
    if (box->height == 0) {
        box->height = MAX (20, extents.height + 2*(box->padding_y)) + 2*box->border_width;
    }
    box->content_position.x = (box->width - extents.width)/2 + extents.x_bearing;
    box->content_position.y = (box->height - extents.height)/2 - extents.y_bearing;
}

void css_box_draw (cairo_t *cr, css_box_t *box, double x, double y)
{
    cairo_save (cr);
    cairo_translate (cr, x, y);

    if (box->width == 0 || box->height == 0) {
        printf ("Drawing a 0 sized box.\n");
    }
    rounded_box_path (cr, 0, 0,box->width+2*box->border_width,
                      box->height+2*box->border_width, box->border_radius);
    cairo_clip (cr);

    cairo_set_source_rgba (cr, box->background_color.r,
                          box->background_color.g,
                          box->background_color.b,
                          box->background_color.a);
    cairo_paint (cr);

    cairo_pattern_t *patt = cairo_pattern_create_linear (box->border_width, box->border_width, 0, box->height);

    if (box->num_gradient_stops > 0) {
        double position;
        double step = 1.0/((double)box->num_gradient_stops-1);
        int stop_idx = 0;
        for (position = 0.0; position<1; position+=step) {
            cairo_pattern_add_color_stop_rgba (patt, position, box->gradient_stops[stop_idx].r,
                                               box->gradient_stops[stop_idx].g,
                                               box->gradient_stops[stop_idx].b,
                                               box->gradient_stops[stop_idx].a);
            stop_idx++;
        }
        cairo_pattern_add_color_stop_rgba (patt, position, box->gradient_stops[stop_idx].r,
                                           box->gradient_stops[stop_idx].g,
                                           box->gradient_stops[stop_idx].b,
                                           box->gradient_stops[stop_idx].a);
        cairo_set_source (cr, patt);
        cairo_paint (cr);
    }

    rounded_box_path (cr, box->border_width/2, box->border_width/2,
                      box->width+box->border_width, box->height+box->border_width,
                      box->border_radius-box->border_width/2);
    cairo_set_source_rgba (cr, box->border_color.r,
                           box->border_color.g,
                           box->border_color.b,
                           box->border_color.a);
    cairo_set_line_width (cr, box->border_width);
    cairo_stroke (cr);

    cairo_move_to (cr, box->content_position.x, box->content_position.y);
    cairo_set_source_rgba (cr, box->color.r, box->color.g, box->color.b,
                          box->color.a);
    cairo_show_text (cr, box->label);

    cairo_restore (cr);
}

void css_box_add_gradient_stops (css_box_t *box, int num_stops, vect4_t *stops)
{
    assert (_g_gui_num_colors+num_stops < ARRAY_SIZE(_g_gui_colors));
    box->num_gradient_stops = num_stops;
    box->gradient_stops = &_g_gui_colors[_g_gui_num_colors];

    int i = 0;
    while (i < num_stops) {
        _g_gui_colors[_g_gui_num_colors] = stops[i];
        _g_gui_num_colors++;
        i++;
    }
}

// TODO: Maybe we don't want content to be inside css_box_t, think this
// through. We are computing information about content every time we draw
// when calling css_box_compute_content_width_and_position(), we won't
// optimize prematureley but it may be a good idea to cache this somehow.
void init_button (css_box_t *box)
{
    *box = (css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 12;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->border_color = RGBA(0, 0, 0, 0.27);
    box->color = RGB(0.2, 0.2, 0.2);

    vect4_t stops[3] = {RGBA(0,0,0,0),
                        RGBA(0,0,0,0),
                        RGBA(0,0,0,0.04)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

void init_pressed_button (css_box_t *box)
{
    *box = (css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 12;
    box->padding_y = 3;
    box->background_color = RGBA(0, 0, 0, 0.05);
    box->border_color = RGBA(0, 0, 0, 0.27);
    box->color = RGB(0.2, 0.2, 0.2);
}

void init_background (css_box_t *box)
{
    *box = (css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 12;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->border_color = RGBA(0, 0, 0, 0.27);
}

void init_text_entry (css_box_t *box)
{
    *box = (css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 3;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->border_color = RGBA(0, 0, 0, 0.25);
    box->color = RGB(0.2, 0.2, 0.2);
    vect4_t stops[2] = {RGB(0.93,0.93,0.93),
                        RGB(0.97,0.97,0.97)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

void init_text_entry_focused (css_box_t *box)
{
    *box = (css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 3;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->border_color = RGBA(0.239216, 0.607843, 0.854902, 0.8);
    box->color = RGB(0.2, 0.2, 0.2);
    vect4_t stops[2] = {RGB(0.93,0.93,0.93),
                        RGB(0.97,0.97,0.97)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

#define GUI_H
#endif
