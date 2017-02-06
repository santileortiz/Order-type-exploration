//gcc -O2 -g -Wall database_expl.c -o bin/database_expl -lcairo -lxcb -lm
// Dependencies:
// sudo apt-get install libxcb1-dev libcairo2-dev
#include <xcb/xcb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo-xcb.h>

#include "common.h"
#include "slo_timers.h"
#include "ot_db.h"
#include "geometry_combinatorics.h"

#define WINDOW_HEIGHT 700
#define WINDOW_WIDTH 700

#define CANVAS_SIZE 65535 // It's the size of the coordinate axis at 100% zoom

typedef struct {
    float time_elapsed_ms;

    bool force_redraw;

    xcb_keycode_t keycode;
    uint16_t modifiers;

    float wheel;
    char mouse_down[3];

    vect2_t ptr;
} app_input_t;

typedef struct {
    cairo_t *cr;
    uint16_t width;
    uint16_t height;
} app_graphics_t;

// Contains information about the layout of the window, and the state of 
// the view area (zoom, position). It's all that's needed to get the
// transform of points from canvas space to windows space.

typedef struct {
    double min_edge;
    double x_margin;
    double y_margin;

    double zoom;
    vect2_t origin;
    double scale_factor;
} view_port_t;

typedef enum {point, triangle, segment} entity_type;
typedef struct {
    entity_type type;
    vect3_t color;
    int line_width;

    // NOTE: Array of indexes into (app_state_t*)st->pts
    int num_points;
    int pts[3];
} entity_t;

typedef enum {
    iterate_order_type,
    iterate_triangle_set,
    iterate_n,
    num_iterator_mode
} iterator_mode_t;

typedef struct {
    int app_is_initialized;
    int n;
    int k;
    order_type_t *ot;
    int db;

    cairo_matrix_t transf;
    app_graphics_t old_graphics;
    double max_zoom;
    double zoom;
    char zoom_changed;
    double old_zoom;
    vect2_t origin;
    view_port_t vp;

    iterator_mode_t it_mode;
    int64_t user_number;

    app_input_t prev_input;
    char maybe_clicked[3];
    char dragging[3];
    vect2_t click_coord[3];
    float time_since_button_press[3];
    float double_click_time;
    float min_distance_for_drag;

    subset_it_t *triangle_it;
    subset_it_t *triangle_set_it;

    int storage_size;
    memory_stack_t memory;

    int num_entities;
    entity_t entities[300];

    int num_points;
    vect2i_t pts[500]; // All figures on canvas are made of points here
} app_state_t;

int index_for_point_or_add (app_state_t *st, vect2i_t p)
{
    int pt_index = 0;
    while (pt_index < st->num_points && st->pts[pt_index].x != p.x) {
        pt_index++;
    }

    if (pt_index == st->num_points) {
        assert (st->num_points < ARRAY_SIZE(st->pts));
        st->pts[pt_index] = p;
        st->num_points++;
    }
    return pt_index;
}

void triangle_entity_add (app_state_t *st, triangle_t *t, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = triangle;
    next_entity->color = color;
    next_entity->num_points = 3;

    int i;
    for (i=0; i<3; i++) {
        next_entity->pts[i] = index_for_point_or_add (st, t->v[i]);
    }
}

void segment_entity_add (app_state_t *st, vect2i_t p1, vect2i_t p2, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = segment;
    next_entity->color = color;
    next_entity->num_points = 2;

    next_entity->pts[0] = index_for_point_or_add (st, p1);
    next_entity->pts[1] = index_for_point_or_add (st, p2);
}

void point_entity_add (app_state_t *st, vect2i_t p, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = point;
    next_entity->color = color;
    next_entity->num_points = 1;

    next_entity->pts[0] = index_for_point_or_add (st, p);
}

void draw_point (cairo_t *cr, double x, double y)
{
    double r = 5, n=0;
    cairo_device_to_user_distance (cr, &r, &n);
    cairo_arc (cr, x, y, r, 0, 2*M_PI);
    cairo_fill (cr);
}

void draw_order_type (cairo_t *cr, order_type_t *ot)
{
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_new_path (cr);

    int i=0;
    while (i < ot->n) {
        draw_point (cr, (double)ot->pts[i].x, (double)ot->pts[i].y);
        i++;
    }

    double r = 1, n=0;
    cairo_device_to_user_distance (cr, &r, &n);
    cairo_set_line_width (cr, r);

    //for (i=0; i<ot->n; i++) {
    //    for (j=i+1; j<ot->n; j++) {
    //        double x1, y1, x2, y2;
    //        cairo_move_to (cr, (double)ot->pts[i].x, (double)ot->pts[i].y);
    //        cairo_line_to (cr, (double)ot->pts[j].x, (double)ot->pts[j].y);
    //        cairo_stroke (cr);
    //    }
    //}
}

void draw_segment (cairo_t *cr, vect2i_t p1, vect2i_t p2, double line_width)
{
    double r = line_width, n=0;
    cairo_device_to_user_distance (cr, &r, &n);
    cairo_set_line_width (cr, r);

    cairo_move_to (cr, (double)p1.x, (double)p1.y);
    cairo_line_to (cr, (double)p2.x, (double)p2.y);
    cairo_stroke (cr);
}

void draw_triangle (cairo_t *cr, triangle_t *t)
{
    cairo_new_path (cr);

    draw_segment (cr, t->v[0], t->v[1], 3);
    draw_segment (cr, t->v[1], t->v[2], 3);
    draw_segment (cr, t->v[2], t->v[0], 3);
}


void draw_entities (app_state_t *st, cairo_t *cr)
{
    int i;
    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case point: {
                vect2i_t p = st->pts[entity->pts[0]];
                draw_point (cr, p.x, p.y);
                } break;
            case segment: {
                vect2i_t p1 = st->pts[entity->pts[0]];
                vect2i_t p2 = st->pts[entity->pts[1]];
                draw_segment (cr, p1, p2, 1);
                } break;
            case triangle: {
                triangle_t t;
                t.v[0] = st->pts[entity->pts[0]];
                t.v[1] = st->pts[entity->pts[1]];
                t.v[2] = st->pts[entity->pts[2]];
                draw_triangle (cr, &t);
                } break;
            default:
                invalid_code_path;
        }
    }
}

#define WINDOW_MARGIN 20
void new_view_port (app_graphics_t graphics, double zoom, vect2_t pt, view_port_t *vp)
{
    if (graphics.height < graphics.width) {
        vp->min_edge = (double)graphics.height;
    } else {
        vp->min_edge = (double)graphics.width;
    }
    vp->x_margin = ((double)graphics.width-vp->min_edge+WINDOW_MARGIN)/2;
    vp->y_margin = ((double)graphics.height-vp->min_edge+WINDOW_MARGIN)/2;

    vp->zoom = zoom;
    vp->origin = pt;

    zoom = vp->zoom/100;
    vp->scale_factor = (vp->min_edge-WINDOW_MARGIN)*zoom/CANVAS_SIZE;
}

void draw_view_area (cairo_t *cr, view_port_t *vp)
{
    cairo_save (cr);
    cairo_identity_matrix (cr);
    cairo_rectangle (cr, vp->x_margin-0.5, vp->y_margin+0.5, vp->min_edge-WINDOW_MARGIN, vp->min_edge-WINDOW_MARGIN);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
    cairo_restore (cr);
}

/*
    printf ("Zoom: %f\n", zoom);
 This function sets the view so that _pt_ (in order type coordinates) is in
 the lower left corner of the view area. _zoom_ is given as a percentage.

   +------------------+    
   +------------------+    
   |               W  |
   |    +-------+     |     W: Window
   |    |       |     |     A: View Area
   |    |   A   |     |     o: position of _pt_
   |    |       |     |
   |    o-------+     |\
   |                  || -> WINDOW_MARGIN
   +------------------+/

*/
void update_transform (app_graphics_t graphics, double zoom, vect2_t pt, view_port_t *new_vp)
{
    cairo_matrix_t transf;
    view_port_t vp;

    new_view_port (graphics, zoom, pt, &vp);

    //draw_view_area (graphics.cr, &vp);

    cairo_matrix_init (&transf,
            vp.scale_factor, 0,
            0, -vp.scale_factor,
            vp.x_margin-pt.x*vp.scale_factor, graphics.height-vp.y_margin+pt.y*vp.scale_factor);
    cairo_set_matrix (graphics.cr, &transf);
    *new_vp = vp;
}

void update_transform_fixed_pt (app_graphics_t graphics, double zoom, vect2_t pt, view_port_t *old_vp, view_port_t *new_vp)
{
    cairo_matrix_t transf;
    view_port_t vp;

    new_view_port (graphics, zoom, pt, &vp);

    //draw_view_area (graphics.cr, &vp);

    double dev_x = pt.x*old_vp->scale_factor+old_vp->x_margin-old_vp->origin.x*old_vp->scale_factor;
    double dev_y = graphics.height - (pt.y*old_vp->scale_factor+old_vp->y_margin-old_vp->origin.y*old_vp->scale_factor);
    cairo_matrix_init (&transf,
            vp.scale_factor, 0,
            0, -vp.scale_factor,
            dev_x-pt.x*vp.scale_factor, dev_y+pt.y*vp.scale_factor);
    cairo_set_matrix (graphics.cr, &transf);
    
    vp.origin.x = vp.x_margin;
    vp.origin.y = graphics.height-vp.y_margin;
    cairo_device_to_user (graphics.cr, &vp.origin.x, &vp.origin.y);
    *new_vp = vp;
}

void focus_order_type (app_graphics_t *graphics, app_state_t *st)
{
    rectangle_t box;
    get_bounding_box (st->ot->pts, st->ot->n, &box);
    int64_t max;
    max_edge (box, &max);
    st->zoom = CANVAS_SIZE*100/((double)max);
    vect2_t new_origin;
    new_origin.x = (double)box.min.x-(max - (box.max.x-box.min.x))/2;
    new_origin.y = (double)box.min.y-(max - (box.max.y-box.min.y))/2;
    update_transform (*graphics, st->zoom, new_origin, &st->vp);
}

int get_tn_for_n (int n)
{
    switch (n) {
        case 4:
            return 1;
        case 5:
            return 2;
        case 6:
            return 4;
        case 7:
            return 7;
        case 8:
            return 8;
        default:
            printf ("Tn is unknown\n");
            return 2;
    }
}

void set_n (app_state_t *st, int n, app_graphics_t *graphics)
{
    st->memory.used = 0; // Clears all storage

    st->n = n;
    st->k = get_tn_for_n (st->n);
    st->ot = order_type_new (10, &st->memory);
    open_database (st->n);
    st->ot->n = st->n;
    db_next (st->ot);

    st->triangle_it = subset_it_new (st->n, 3, &st->memory);
    subset_it_precompute (st->triangle_it);
    st->triangle_set_it = subset_it_new (st->triangle_it->size, st->k, &st->memory);
    focus_order_type (graphics, st);
}


int end_execution = 0;

bool update_and_render (app_graphics_t *graphics, app_input_t input)
{
    static app_state_t *st = NULL;
    char redraw = 0;

    if (!st) {
        int storage_size =  megabyte(60);
        uint8_t* memory = calloc (storage_size, 1);
        st = (app_state_t*)memory;
        st->storage_size = storage_size;
        memory_stack_init (&st->memory, st->storage_size-sizeof(app_state_t), memory+sizeof(app_state_t));

        st->n = 8;
        st->k = get_tn_for_n (st->n);

        st->it_mode = iterate_order_type;
        st->user_number = -1;

        st->max_zoom = 500000;
        st->zoom = 100;
        st->zoom_changed = 0;
        st->old_zoom = 100;
        st->origin = VECT2(0,0);
        st->app_is_initialized = 1;
        st->old_graphics = *graphics;
        st->prev_input = input;
        st->time_since_button_press[0] = -1;
        st->time_since_button_press[1] = -1;
        st->time_since_button_press[2] = -1;
        st->double_click_time = 100;
        st->min_distance_for_drag = 10;

        set_n (st, st->n, graphics);

        redraw = 1;
    }

    if (10 <= input.keycode && input.keycode < 20) { // KEY_0-9
        if (st->user_number ==-1) {
            st->user_number++;
        }

        int num = (input.keycode+1)%10;
        st->user_number = st->user_number*10 + num; 
        printf ("Input: %"PRIi64"\n", st->user_number);
        input.keycode = 0;
    }

    switch (input.keycode) {
        case 9: //KEY_ESC
            if (st->user_number != -1) {
                st->user_number = -1;
            } else {
                end_execution = 1;
            }
            break;
        case 24: //KEY_Q
            end_execution = 1;
            return false;
        case 33: //KEY_P
            print_order_type (st->ot);
            break;
        case 116://KEY_DOWN_ARROW
            if (st->zoom<st->max_zoom) {
                st->zoom += 10;
                st->zoom_changed = 1;
                redraw = 1;
            }
            break;
        case 111://KEY_UP_ARROW
            if (st->zoom>10) {
                st->zoom -= 10;
                st->zoom_changed = 1;
                redraw = 1;
            }
            break;
        case 23: //KEY_TAB
            st->it_mode = (st->it_mode+1)%num_iterator_mode;
            switch (st->it_mode) {
                case iterate_order_type:
                    printf ("Iterating order types\n");
                    break;
                case iterate_triangle_set:
                    printf ("Iterating triangle sets\n");
                    break;
                case iterate_n:
                    printf ("Iterating n\n");
                    break;
                default:
                    break;
            }
            break;
        case 57: //KEY_N
        case 113://KEY_LEFT_ARROW
        case 114://KEY_RIGHT_ARROW
            if (st->it_mode == iterate_order_type) {
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    db_prev (st->ot);
                } else {
                    db_next (st->ot);
                }
            } else if (st->it_mode == iterate_n) {
                int n = st->n;
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    n--;
                    if (n<3) {
                        n=10;
                    }
                } else {
                    n++;
                    if (n>10) {
                        n=3;
                    }
                }
                set_n (st, n, graphics);
            } else if (st->it_mode == iterate_triangle_set) {
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    subset_it_prev (st->triangle_set_it);
                } else {
                    subset_it_next (st->triangle_set_it);
                }
            }
            redraw = 1;
            break;
        case 36: //KEY_ENTER
            if (st->it_mode == iterate_order_type) {
                db_seek (st->ot, st->user_number);
            } else if (st->it_mode == iterate_n) {
                if (3 <= st->user_number && st->user_number <= 10) {
                    set_n (st, st->user_number, graphics);
                }
            } else if (st->it_mode == iterate_triangle_set) {
                subset_it_seek (st->triangle_set_it, st->user_number);
            }
            st->user_number = -1;
            redraw = 1;
            break;
        default:
            if (input.keycode >= 8) {
                printf ("%" PRIu8 "\n", input.keycode);
                //printf ("%" PRIu16 "\n", input.modifiers);
            }
            break;
    }

    if (input.wheel != 1) {
        st->zoom *= input.wheel;
        if (st->zoom > st->max_zoom) {
            st->zoom = st->max_zoom;
        }
        st->zoom_changed = 1;
        redraw = 1;
    }

    // TODO: This is the only way I could think of to differentiate a drag
    // inside the window, from a resize of the window.
    // NOTE: I did it this way because while dragging to resize the window, on
    // some frames the width and the height don't change and the pointer is
    // inside the window (when shrinking).
    static int resizing = 0;
    if ((st->dragging[0] && resizing) ||
            graphics->width != st->old_graphics.width || graphics->height != st->old_graphics.height) {
        resizing = 1;
    }

    // TODO: This is a very rudimentary implementation for detection of Click,
    // Double Click, and Dragging. See if it can be implemented cleanly with a
    // state machine.
    if (input.mouse_down[0]) {
        if (!st->prev_input.mouse_down[0]) {
            st->click_coord[0] = input.ptr;
            if (st->time_since_button_press[0] > 0 && st->time_since_button_press[0] < st->double_click_time) {
                // DOUBLE CLICK
                focus_order_type (graphics, st);
                redraw = 1;

                // We want to ignore this button press as an actual click, we
                // use -10 to signal this.
                st->time_since_button_press[0] = -10;
            }
        } else {
            // button is being held
            if (st->dragging[0] || vect2_distance (&input.ptr, &st->click_coord[0]) > st->min_distance_for_drag) {
                if (!resizing) {
                    // DRAGGING
                    vect2_t new_origin;
                    new_origin.x = input.ptr.x - st->prev_input.ptr.x;
                    new_origin.y = -(input.ptr.y - st->prev_input.ptr.y);
                    cairo_device_to_user_distance (graphics->cr, &new_origin.x, &new_origin.y);
                    new_origin.x = st->vp.origin.x - new_origin.x;
                    new_origin.y = st->vp.origin.y + new_origin.y;
                    update_transform (*graphics, st->zoom, new_origin, &st->vp);
                    redraw = 1;

                    st->dragging[0] = 1;
                    st->time_since_button_press[0] = -10;
                }
            }
        }
    } else {
        if (st->prev_input.mouse_down[0]) {
            if (st->time_since_button_press[0] != -10) {
                // button was released, start counter to see if
                // it's a double click
                st->time_since_button_press[0] = 0;
                // We can't separate a click from a double click just now, so we set
                // this and wait to see if st->double_click_time passes with no
                // other button presses.
                st->maybe_clicked[0] = 1;
            } else {
                st->time_since_button_press[0] = -1;
            }
        } else if (st->time_since_button_press[0] >= 0) {
            st->time_since_button_press[0] += input.time_elapsed_ms;
            if (st->maybe_clicked && st->time_since_button_press[0] > st->double_click_time) {
                // CLICK
                printf ("Click\n");
                st->time_since_button_press[0] = -1;
            }
        }
        resizing = 0;
        st->dragging[0] = 0;
    }
    st->prev_input = input;

    if (redraw || input.force_redraw) {
        cairo_t *cr = graphics->cr;
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_paint (cr);

        if (st->old_graphics.width != graphics->width || st->old_graphics.height != graphics->height) {
            update_transform (*graphics, st->zoom, st->vp.origin, &st->vp);
        } else if (st->zoom_changed) {
            view_port_t new_vp;
            vect2_t canvas_ptr = input.ptr;
            cairo_device_to_user (graphics->cr, &canvas_ptr.x, &canvas_ptr.y);

            update_transform_fixed_pt (*graphics, st->zoom, canvas_ptr, &st->vp, &new_vp);
            st->vp = new_vp;
            st->zoom_changed = 0;
        }

        st->old_graphics = *graphics;

        // Construct drawing on canvas.
        st->num_entities = 0;
        st->num_points = 0;

        int i;
#if 1
        for (i=0; i<st->n-1; i++) {
            int j;
            for (j=i+1; j<st->n; j++) {
                order_type_t *ot = st->ot;
                segment_entity_add (st, ot->pts[i], ot->pts[j], VECT3(0,0,0));
            }
        }
#endif

        get_next_color (0);

        for (i=0; i<st->k; i++) {
            triangle_t t;
            subset_it_seek (st->triangle_it, st->triangle_set_it->idx[i]);
            t.v[0] = st->ot->pts[st->triangle_it->idx[0]];
            t.v[1] = st->ot->pts[st->triangle_it->idx[1]];
            t.v[2] = st->ot->pts[st->triangle_it->idx[2]];

            vect3_t color;
            get_next_color (&color);
            triangle_entity_add (st, &t, color);
        }


        for (i=0; i<st->n; i++) {
            order_type_t *ot = st->ot;
            point_entity_add (st, ot->pts[i], VECT3(0,0,0));
        }

        redraw = 0;

        draw_entities (st, cr);
        cairo_surface_flush (cairo_get_target(cr));
        return true;
    } else {
        return false;
    }
}

int main (void)
{
    // Setup clocks
    setup_clocks ();

    //////////////////
    // X11 setup

    xcb_connection_t *connection = xcb_connect (NULL, NULL);

    /* Get the first screen */
    // TODO: xcb_connect() returns default screen on 2nd argument, maybe
    // use that instead of assuming it's 0.
    // TODO: What happens if there is more than 1 screen?, probably will
    // have to iterate with xcb_setup_roots_iterator(), and xcb_screen_next ().
    xcb_screen_t *screen = xcb_setup_roots_iterator (xcb_get_setup (connection)).data;

    // Create a window
    xcb_drawable_t  window = xcb_generate_id (connection);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {screen->white_pixel,
            XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|
            XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_BUTTON_PRESS|
            XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION};

    xcb_create_window (connection,         /* connection          */
            XCB_COPY_FROM_PARENT,          /* depth               */
            window,                        /* window Id           */
            screen->root,                  /* parent window       */
            0, 0,                          /* x, y                */
            WINDOW_WIDTH, WINDOW_HEIGHT,   /* width, height       */
            10,                            /* border_width        */
            XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
            XCB_COPY_FROM_PARENT,          /* visual              */
            mask, values);                 /* masks */

    // Set window title
    char *title = "Order Type";
    xcb_change_property (connection,
            XCB_PROP_MODE_REPLACE,
            window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            strlen (title),
            title);

    // Getting visual of root window (required to get a cairo surface)
    xcb_depth_iterator_t depth_iter;
    xcb_visualtype_t  *root_window_visual = NULL;

    depth_iter = xcb_screen_allowed_depths_iterator (screen);
    for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator (depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next (&visual_iter)) {
            if (screen->root_visual == visual_iter.data->visual_id) {
                root_window_visual = visual_iter.data;
                break;
            }
        }
    }

    xcb_gcontext_t  gc = xcb_generate_id (connection);
    xcb_pixmap_t backbuffer = xcb_generate_id (connection);
    xcb_create_gc (connection, gc, window, 0, values);
    xcb_create_pixmap (connection,
            screen->root_depth,     /* depth of the screen */
            backbuffer,  /* id of the pixmap */
            window,
            WINDOW_WIDTH,     /* pixel width of the window */
            WINDOW_HEIGHT);  /* pixel height of the window */

    xcb_map_window (connection, window);
    xcb_flush (connection);

    cairo_surface_t *surface = cairo_xcb_surface_create (connection, backbuffer, root_window_visual, WINDOW_WIDTH, WINDOW_HEIGHT);
    cairo_t *cr = cairo_create (surface);

    // ////////////////
    // Main event loop
    xcb_generic_event_t *event;
    app_graphics_t graphics;
    graphics.cr = cr;
    graphics.width = WINDOW_WIDTH;
    graphics.height = WINDOW_HEIGHT;
    uint16_t pixmap_width = WINDOW_WIDTH;
    uint16_t pixmap_height = WINDOW_HEIGHT;

    float frame_rate = 60;
    float target_frame_length_ms = 1000/(frame_rate);
    struct timespec start_ticks;
    struct timespec end_ticks;

    clock_gettime(CLOCK_MONOTONIC, &start_ticks);
    app_input_t app_input = {0};
    app_input.wheel = 1;
    while (!end_execution) {
        // Get pointer state, this is how we detect button releases, because we
        // ignore button_release events so we don't miss button presses shorter
        // than 1 frame.
        xcb_generic_error_t *err = NULL;
        xcb_query_pointer_cookie_t ck = xcb_query_pointer (connection, window);
        xcb_query_pointer_reply_t *ptr_state = xcb_query_pointer_reply (connection, ck, &err);
        if (err != NULL) {
            printf ("Error while polling pointer\n");
        }
        app_input.mouse_down[0] = (XCB_KEY_BUT_MASK_BUTTON_1 & ptr_state->mask) ? 1 : 0;
        app_input.mouse_down[1] = (XCB_KEY_BUT_MASK_BUTTON_2 & ptr_state->mask) ? 1 : 0;
        app_input.mouse_down[2] = (XCB_KEY_BUT_MASK_BUTTON_3 & ptr_state->mask) ? 1 : 0;
        if (ptr_state->win_x > 0 && ptr_state->win_x < graphics.width &&
            ptr_state->win_y > 0 && ptr_state->win_y < graphics.height) {
            app_input.ptr.x = ptr_state->win_x;
            app_input.ptr.y = ptr_state->win_y;
        }
        free (ptr_state);
        while ((event = xcb_poll_for_event (connection))) {
            switch (event->response_type) {
                case XCB_CONFIGURE_NOTIFY: {
                    uint16_t new_width = ((xcb_configure_notify_event_t*)event)->width;
                    uint16_t new_height = ((xcb_configure_notify_event_t*)event)->height;
                    if (new_width > pixmap_width || new_height > pixmap_height) {
                        pixmap_width = new_width;
                        pixmap_height = new_height;
                        xcb_free_pixmap (connection, backbuffer);
                        backbuffer = xcb_generate_id (connection);
                        xcb_create_pixmap (connection, screen->root_depth, backbuffer, window,
                                pixmap_width, pixmap_height);
                        cairo_xcb_surface_set_drawable (cairo_get_target (graphics.cr), backbuffer,
                                pixmap_width, pixmap_height);
                    }
                    graphics.width = new_width;
                    graphics.height = new_height;
                    } break;
                case XCB_MOTION_NOTIFY: {
                    app_input.ptr.x = ((xcb_motion_notify_event_t*)event)->event_x;
                    app_input.ptr.y = ((xcb_motion_notify_event_t*)event)->event_y;
                    } break;
                case XCB_KEY_PRESS:
                    app_input.keycode = ((xcb_key_press_event_t*)event)->detail;
                    app_input.modifiers = ((xcb_key_press_event_t*)event)->state;
                    break;
                case XCB_EXPOSE:
                    // We should tell which areas need exposing
                    printf ("cnt: %"PRIu16"\n", ((xcb_expose_event_t*)event)->count); 
                    app_input.force_redraw = 1;
                    break;
                case XCB_BUTTON_PRESS:
                    if (((xcb_key_press_event_t*)event)->detail == 4) {
                        app_input.wheel *= 1.2;
                    } else if (((xcb_key_press_event_t*)event)->detail == 5) {
                        app_input.wheel /= 1.2;
                    } else {
                        app_input.mouse_down[((xcb_button_press_event_t*)event)->detail-1] = 1;
                    }
                    break;
                case XCB_BUTTON_RELEASE:
                    break;
                default: 
                    /* Unknown event type, ignore it */
                    continue;
            }
            free (event);
        }

        // TODO: How bad is this? should we actually measure it?
        app_input.time_elapsed_ms = target_frame_length_ms;
        bool blit_needed = update_and_render (&graphics, app_input);
        clock_gettime (CLOCK_MONOTONIC, &end_ticks);
        float time_elapsed = time_elapsed_in_ms (&start_ticks, &end_ticks);
        if (time_elapsed < target_frame_length_ms) {
            struct timespec sleep_ticks;
            sleep_ticks.tv_sec = 0;
            sleep_ticks.tv_nsec = (long)((target_frame_length_ms-time_elapsed)*1000000);
            nanosleep (&sleep_ticks, NULL);
        } else {
            printf ("Frame missed! %f ms elapsed\n", time_elapsed);
        }

        if (blit_needed) {
            xcb_copy_area (connection,
                    backbuffer,  /* drawable we want to paste */
                    window, /* drawable on which we copy the previous Drawable */
                    gc,
                    0,0,0,0,
                    graphics.width,         /* pixel width of the region we want to copy */
                    graphics.height);      /* pixel height of the region we want to copy */
        }

        clock_gettime(CLOCK_MONOTONIC, &end_ticks);
        //printf ("FPS: %f\n", 1000/time_elapsed_in_ms (&start_ticks, &end_ticks));
        start_ticks = end_ticks;

        xcb_flush (connection);
        app_input.keycode = 0;
        app_input.wheel = 1;
        app_input.force_redraw = 0;
    }

    // These don't seem to free everything according to Valgrind, so we don't
    // care to free them, the process will end anyway.
    // cairo_surface_destroy(surface);
    // cairo_destroy (cr);
    // xcb_disconnect (connection);

    return 0;
}

