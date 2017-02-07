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
    double scale;
    double dx;
    double dy;
} transf_t;

// canvas coordinates -> window pixels
void transform (transf_t tr, vect2_t *p)
{
    p->x = tr.scale*p->x+tr.dx;
    p->y = tr.scale*(CANVAS_SIZE-p->y)+tr.dy;
}

// window pixels -> canvas coordinates
void transform_inv (transf_t tr, vect2_t *p)
{
    p->x = (p->x - tr.dx)/tr.scale;
    p->y = (tr.scale*CANVAS_SIZE + tr.dy - p->y )/(tr.scale);
}

typedef struct {
    cairo_t *cr;
    uint16_t width;
    uint16_t height;
    transf_t T;
} app_graphics_t;

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

void draw_point (app_graphics_t *graphics, vect2i_t p)
{
    cairo_t *cr = graphics->cr;
    vect2_t p_dev = v2_from_v2i (p);
    transform (graphics->T, &p_dev);
    cairo_arc (cr, p_dev.x, p_dev.y, 5, 0, 2*M_PI);
    cairo_fill (cr);
}

void draw_segment (app_graphics_t *graphics, vect2i_t p1, vect2i_t p2, double line_width)
{
    cairo_t *cr = graphics->cr;
    cairo_set_line_width (cr, line_width);

    vect2_t p1_dev = v2_from_v2i (p1);
    transform (graphics->T, &p1_dev);

    vect2_t p2_dev = v2_from_v2i (p2);
    transform (graphics->T, &p2_dev);

    cairo_move_to (cr, (double)p1_dev.x, (double)p1_dev.y);
    cairo_line_to (cr, (double)p2_dev.x, (double)p2_dev.y);
    cairo_stroke (cr);
}

void draw_triangle (app_graphics_t *graphics, triangle_t *t)
{
    draw_segment (graphics, t->v[0], t->v[1], 3);
    draw_segment (graphics, t->v[1], t->v[2], 3);
    draw_segment (graphics, t->v[2], t->v[0], 3);
}

void draw_entities (app_state_t *st, app_graphics_t *graphics)
{
    int i;
    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (graphics->cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case point: {
                vect2i_t p = st->pts[entity->pts[0]];
                draw_point (graphics, p);
                } break;
            case segment: {
                vect2i_t p1 = st->pts[entity->pts[0]];
                vect2i_t p2 = st->pts[entity->pts[1]];
                draw_segment (graphics, p1, p2, 1);
                } break;
            case triangle: {
                triangle_t t;
                t.v[0] = st->pts[entity->pts[0]];
                t.v[1] = st->pts[entity->pts[1]];
                t.v[2] = st->pts[entity->pts[2]];
                draw_triangle (graphics, &t);
                } break;
            default:
                invalid_code_path;
        }
    }
}

// Here _pos_ represents the position in window pixels of the point
// (0, CANVAS_SIZE) of the canvas. Measured from the top left corner.
#define WINDOW_MARGIN 40
void change_zoom (app_graphics_t *graphics, double zoom, vect2_t pt)
{
    zoom = zoom/100;
    double new_scale = (double)(graphics->height-2*WINDOW_MARGIN)*zoom/CANVAS_SIZE;
    graphics->T.dy = pt.y - ((pt.y-graphics->T.dy)*new_scale/graphics->T.scale);
    graphics->T.dx = pt.x - ((pt.x-graphics->T.dx)*new_scale/graphics->T.scale);
    graphics->T.scale = new_scale;
}

void focus_order_type (app_graphics_t *graphics, app_state_t *st)
{
    rectangle_t box;
    get_bounding_box (st->ot->pts, st->ot->n, &box);
    double box_aspect_ratio = (double)(box.max.x-box.min.x)/(box.max.y-box.min.y);
    double window_aspect_ratio = (double)(graphics->width)/(graphics->height);
    double y_margin, x_margin;
    if (box_aspect_ratio < window_aspect_ratio) {
        st->zoom = CANVAS_SIZE*100/((double)(box.max.y - box.min.y));
        graphics->T.scale = (graphics->height - 2*WINDOW_MARGIN)*st->zoom/(100*CANVAS_SIZE);
        y_margin = WINDOW_MARGIN;
        x_margin = (graphics->width - (box.max.x-box.min.x)*graphics->T.scale)/2;
    } else {
        st->zoom = CANVAS_SIZE*100/((double)(box.max.x - box.min.x));
        graphics->T.scale = (graphics->width - 2*WINDOW_MARGIN)*st->zoom/(100*CANVAS_SIZE);
        x_margin = WINDOW_MARGIN;
        y_margin = (graphics->height - (box.max.y-box.min.y)*graphics->T.scale)/2;
    }

    graphics->T.dx = x_margin - (box.min.x)*graphics->T.scale;
    graphics->T.dy = y_margin - (CANVAS_SIZE-box.max.y)*graphics->T.scale;
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
                    graphics->T.dx += input.ptr.x - st->prev_input.ptr.x;
                    graphics->T.dy += (input.ptr.y - st->prev_input.ptr.y);
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

        // TODO: Handle window resizing to scale canvas correctly.
        if (st->zoom_changed) {
            change_zoom (graphics, st->zoom, input.ptr);
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
            draw_point (graphics, ot->pts[i]);
        }

        //printf ("dx: %f, dy: %f, s: %f, z: %f\n", graphics->T.dx, graphics->T.dy, graphics->T.scale, st->zoom);
        draw_entities (st, graphics);
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
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[2] = {XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|
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
    graphics.T.dx = WINDOW_MARGIN;
    graphics.T.dy = WINDOW_MARGIN;
    graphics.T.scale = (double)(graphics.height-2*WINDOW_MARGIN)/CANVAS_SIZE;
    uint16_t pixmap_width = WINDOW_WIDTH;
    uint16_t pixmap_height = WINDOW_HEIGHT;
    bool make_pixmap_bigger = false;

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
                        make_pixmap_bigger = 1;
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

        if (make_pixmap_bigger) {
            pixmap_width = graphics.width;
            pixmap_height = graphics.height;
            xcb_free_pixmap (connection, backbuffer);
            backbuffer = xcb_generate_id (connection);
            xcb_create_pixmap (connection, screen->root_depth, backbuffer, window,
                    pixmap_width, pixmap_height);
            cairo_xcb_surface_set_drawable (cairo_get_target (graphics.cr), backbuffer,
                    pixmap_width, pixmap_height);
            make_pixmap_bigger = false;
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

