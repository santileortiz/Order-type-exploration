/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

// Dependencies:
// sudo apt-get install libxcb1-dev libcairo2-dev
#include <X11/Xlib-xcb.h>
#include <xcb/sync.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>
#include <curl/curl.h>

#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
//#define NDEBUG
#include <assert.h>
#include <errno.h>

#include "common.h"
#include "gui.h"
#include "slo_timers.h"
#include "order_types.h"

#include "geometry_combinatorics.h"
#include "tree_mode.h"
#include "grid_mode.h"
#include "point_set_mode.h"
#include "app_api.h"

#define SEQUENCE_STORE_IMPL
#include "sequence_store.h"

// We do a unity build for now
#include "order_types.c"
#include "tree_mode.c"
#include "grid_mode.c"
#include "point_set_mode.c"

#define WINDOW_HEIGHT 700
#define WINDOW_WIDTH 700

bool update_and_render (struct app_state_t *st, app_graphics_t *graphics, app_input_t input)
{
    if (!st->is_initialized) {
        st->end_execution = false;
        st->is_initialized = true;

        st->time_since_last_click[0] = -1;
        st->time_since_last_click[1] = -1;
        st->time_since_last_click[2] = -1;
        st->double_click_time = 100;
        st->min_distance_for_drag = 10;

        init_button (&st->css_styles[CSS_BUTTON]);
        init_button_active (&st->css_styles[CSS_BUTTON_ACTIVE]);
        st->css_styles[CSS_BUTTON].selector_active = &st->css_styles[CSS_BUTTON_ACTIVE];

        init_suggested_action_button (&st->css_styles[CSS_BUTTON_SA]);
        init_suggested_action_button_active (&st->css_styles[CSS_BUTTON_SA_ACTIVE]);
        st->css_styles[CSS_BUTTON_SA].selector_active = &st->css_styles[CSS_BUTTON_SA_ACTIVE];

        init_background (&st->css_styles[CSS_BACKGROUND]);
        init_text_entry (&st->css_styles[CSS_TEXT_ENTRY]);
        init_text_entry_focused (&st->css_styles[CSS_TEXT_ENTRY_FOCUS]);
        init_label (&st->css_styles[CSS_LABEL]);
        init_title_label (&st->css_styles[CSS_TITLE_LABEL]);
        st->focused_layout_box = -1;

        st->app_mode = APP_POINT_SET_MODE;

        input.force_redraw = 1;
    }

    st->temporary_memory.used = 0;

    // TODO: This is a very rudimentary implementation for detection of Click,
    // Double Click, and Dragging. See if it can be implemented cleanly with a
    // state machine.
    app_input_t prev_input = st->input;
    if (input.mouse_down[0]) {
        if (!prev_input.mouse_down[0]) {
            st->click_coord[0] = input.ptr;
            if (st->time_since_last_click[0] > 0 && st->time_since_last_click[0] < st->double_click_time) {
                // DOUBLE CLICK
                st->mouse_double_clicked[0] = true;

                // We want to ignore this button press as an actual click, we
                // use -10 to signal this.
                st->time_since_last_click[0] = -10;
            }
        } else {
            // button is being held
            if (st->dragging[0] || vect2_distance (&input.ptr, &st->click_coord[0]) > st->min_distance_for_drag) {
                // DRAGGING
                st->dragging[0] = 1;
                st->time_since_last_click[0] = -10;
            }
        }
    } else {
        if (prev_input.mouse_down[0]) {
            // CLICK
            st->mouse_clicked[0] = true;

            st->mouse_double_clicked[0] = false;
            if (st->time_since_last_click[0] != -10) {

                // button was released, start counter to see if
                // it's a double click
                st->time_since_last_click[0] = 0;
            } else {
                st->time_since_last_click[0] = -1;
            }
        } else if (st->time_since_last_click[0] >= 0) {
            st->mouse_clicked[0] = false;
            st->time_since_last_click[0] += input.time_elapsed_ms;
            if (st->time_since_last_click[0] > st->double_click_time) {
                st->time_since_last_click[0] = -1;
            }
        } else {
            st->mouse_clicked[0] = false;
        }
        st->dragging[0] = 0;
    }

    st->ptr_delta.x = input.ptr.x - prev_input.ptr.x;
    st->ptr_delta.y = input.ptr.y - prev_input.ptr.y;

    switch (input.keycode) {
        case 58: //KEY_M
            st->app_mode = (st->app_mode + 1)%NUM_APP_MODES;
            input.force_redraw = true;
            break;
        case 24: //KEY_Q
            st->end_execution = 1;
            return false;
        default:
            //if (input.keycode >= 8) {
            //    printf ("%" PRIu8 "\n", input.keycode);
            //    //printf ("%" PRIu16 "\n", input.modifiers);
            //}
            break;
    }

    st->input = input;
    bool blit_needed = false;
    switch (st->app_mode) {
        case APP_POINT_SET_MODE:
            blit_needed = point_set_mode (st, graphics);
            break;
        case APP_GRID_MODE:
            blit_needed = grid_mode (st, graphics);
            break;
        case APP_TREE_MODE:
            blit_needed = tree_mode (st, graphics);
            break;
        default:
            invalid_code_path;
    }
    cairo_surface_flush (cairo_get_target(graphics->cr));
    return blit_needed;
}

xcb_atom_t get_x11_atom (xcb_connection_t *c, const char *value)
{
    xcb_atom_t res = XCB_ATOM_NONE;
    xcb_generic_error_t *err = NULL;
    xcb_intern_atom_cookie_t ck = xcb_intern_atom (c, 0, strlen(value), value);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error while requesting atom.\n");
        free (err);
    }
    res = reply->atom;
    free(reply);
    return res;
}

char* get_x11_atom_name (xcb_connection_t *c, xcb_atom_t atom)
{
    xcb_get_atom_name_cookie_t ck = xcb_get_atom_name (c, atom);
    xcb_generic_error_t *err = NULL;
    xcb_get_atom_name_reply_t * reply = xcb_get_atom_name_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error while requesting atom's name.\n");
        free (err);
    }

    return xcb_get_atom_name_name (reply);
}

void increment_sync_counter (xcb_sync_int64_t *counter)
{
    counter->lo++;
    if (counter->lo == 0) {
        counter->hi++;
    }
}

xcb_visualtype_t* get_visual_struct_from_visualid (xcb_connection_t *c, xcb_screen_t *screen, xcb_visualid_t id)
{
    xcb_visualtype_t  *visual_type = NULL;    /* the returned visual type */

    /* you init the connection and screen_nbr */

    xcb_depth_iterator_t depth_iter;
    if (screen) {
        depth_iter = xcb_screen_allowed_depths_iterator (screen);
        for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
            xcb_visualtype_iterator_t visual_iter;

            visual_iter = xcb_depth_visuals_iterator (depth_iter.data);
            for (; visual_iter.rem; xcb_visualtype_next (&visual_iter)) {
                if (id == visual_iter.data->visual_id) {
                    visual_type = visual_iter.data;
                    //break;
                }
            }
        }
    }
    return visual_type;
}

xcb_visualtype_t* get_visual_max_depth (xcb_connection_t *c, xcb_screen_t *screen, uint8_t *found_depth)
{
    xcb_visualtype_t  *visual_type = NULL;    /* the returned visual type */

    /* you init the connection and screen_nbr */

    *found_depth = 0;
    xcb_depth_iterator_t depth_iter;
    if (screen) {
        depth_iter = xcb_screen_allowed_depths_iterator (screen);
        for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
            xcb_visualtype_iterator_t visual_iter;
            visual_iter = xcb_depth_visuals_iterator (depth_iter.data);
            if (visual_iter.rem) {
                if (*found_depth < depth_iter.data->depth) {
                    *found_depth = depth_iter.data->depth;
                    visual_type = visual_iter.data;
                }
            }
        }
    }
    return visual_type;
}

Visual* Visual_from_visualid (Display *dpy, xcb_visualid_t visualid)
{
    XVisualInfo templ = {0};
    templ.visualid = visualid;
    int n;
    XVisualInfo *found = XGetVisualInfo (dpy, VisualIDMask, &templ, &n);
    return found->visual;
}

struct x_state {
    xcb_connection_t *xcb_c;
    Display *xlib_dpy;

    xcb_screen_t *screen;
    xcb_visualtype_t *visual;
    uint8_t depth;

    xcb_drawable_t window;
    xcb_pixmap_t backbuffer;

    xcb_gcontext_t gc;

    xcb_atom_t wm_protocols_a;
    xcb_atom_t delete_window_a;

    xcb_sync_counter_t counters[2];
    xcb_sync_int64_t counter_val;
    xcb_atom_t net_wm_sync_a;
    xcb_atom_t net_wm_frame_drawn_a;
    xcb_atom_t net_wm_frame_timings_a;
};

void x11_create_window (struct x_state *x_st, char *title)
{
    uint32_t event_mask = XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|
                             XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_BUTTON_PRESS|
                             XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION;
    uint32_t mask = XCB_CW_EVENT_MASK;

    x_st->visual = get_visual_max_depth (x_st->xcb_c, x_st->screen, &x_st->depth);

    // Boilerplate code required to get maximum depth available
    // 
    // These attributes are required to have different depth than the root
    // window. Avoids inheriting pixmaps with depths that don't match.

    mask |= XCB_CW_BORDER_PIXEL|XCB_CW_COLORMAP; 
    xcb_colormap_t colormap = xcb_generate_id (x_st->xcb_c);
    xcb_create_colormap (x_st->xcb_c, XCB_COLORMAP_ALLOC_NONE,
                         colormap, x_st->screen->root, x_st->visual->visual_id); 


    uint32_t values[] = {// , // XCB_CW_BACK_PIXMAP
                         // , // XCB_CW_BACK_PIXEL
                         // , // XCB_CW_BORDER_PIXMAP
                         0  , // XCB_CW_BORDER_PIXEL
                         // , // XCB_CW_BIT_GRAVITY
                         // , // XCB_CW_WIN_GRAVITY
                         // , // XCB_CW_BACKING_STORE
                         // , // XCB_CW_BACKING_PLANES
                         // , // XCB_CW_BACKING_PIXEL     
                         // , // XCB_CW_OVERRIDE_REDIRECT
                         // , // XCB_CW_SAVE_UNDER
                         event_mask, //XCB_CW_EVENT_MASK
                         // , // XCB_CW_DONT_PROPAGATE
                         colormap, // XCB_CW_COLORMAP
                         //  // XCB_CW_CURSOR
                         };

    x_st->window = xcb_generate_id (x_st->xcb_c);
    xcb_create_window (x_st->xcb_c,        /* connection          */
            x_st->depth,                   /* depth               */
            x_st->window,                  /* window Id           */
            x_st->screen->root,            /* parent window       */
            0, 0,                          /* x, y                */
            WINDOW_WIDTH, WINDOW_HEIGHT,   /* width, height       */
            0,                             /* border_width        */
            XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
            x_st->visual->visual_id,       /* visual              */
            mask, values);                 /* masks */

    // Set window title
    xcb_change_property (x_st->xcb_c,
            XCB_PROP_MODE_REPLACE,
            x_st->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            strlen (title),
            title);
}

void x11_setup_icccm_and_ewmh_protocols (struct x_state *x_st)
{
    xcb_atom_t net_wm_protocols_value[2];

    /////////////////////////////
    // WM_DELETE_WINDOW protocol
    x_st->delete_window_a = get_x11_atom (x_st->xcb_c, "WM_DELETE_WINDOW");

    /////////////////////////////////
    // _NET_WM_SYNC_REQUEST protocol
    x_st->net_wm_sync_a = get_x11_atom (x_st->xcb_c, "_NET_WM_SYNC_REQUEST");
    // NOTE: Following are used to recognize the ClientMessage type.
    x_st->net_wm_frame_drawn_a = get_x11_atom (x_st->xcb_c, "_NET_WM_FRAME_DRAWN");
    x_st->net_wm_frame_timings_a = get_x11_atom (x_st->xcb_c, "_NET_WM_FRAME_TIMINGS");

    // Set up counters for extended mode
    x_st->counter_val = (xcb_sync_int64_t){0, 0}; //{HI, LO}
    x_st->counters[0] = xcb_generate_id (x_st->xcb_c);
    xcb_sync_create_counter (x_st->xcb_c, x_st->counters[0], x_st->counter_val);

    x_st->counters[1] = xcb_generate_id (x_st->xcb_c);
    xcb_sync_create_counter (x_st->xcb_c, x_st->counters[1], x_st->counter_val);

    xcb_change_property (x_st->xcb_c,
            XCB_PROP_MODE_REPLACE,
            x_st->window,
            get_x11_atom (x_st->xcb_c, "_NET_WM_SYNC__REQUEST_COUNTER"),
            XCB_ATOM_CARDINAL,
            32,
            ARRAY_SIZE(x_st->counters),
            x_st->counters);

    /////////////////////////////
    // Set WM_PROTOCOLS property
    x_st->wm_protocols_a = get_x11_atom (x_st->xcb_c, "WM_PROTOCOLS");
    net_wm_protocols_value[0] = x_st->delete_window_a;
    net_wm_protocols_value[1] = x_st->net_wm_sync_a;

    xcb_change_property (x_st->xcb_c,
            XCB_PROP_MODE_REPLACE,
            x_st->window,
            x_st->wm_protocols_a,
            XCB_ATOM_ATOM,
            32,
            ARRAY_SIZE(net_wm_protocols_value),
            net_wm_protocols_value);
}

void blocking_xcb_sync_set_counter (xcb_connection_t *c, xcb_sync_counter_t counter, xcb_sync_int64_t *val)
{
    xcb_void_cookie_t ck = xcb_sync_set_counter_checked (c, counter, *val);
    xcb_generic_error_t *error; 
    if ((error = xcb_request_check(c, ck))) { 
        printf("Error setting counter %d\n", error->error_code); 
        free(error); 
    }
}

void x11_notify_start_of_frame (struct x_state *x_st)
{
    increment_sync_counter (&x_st->counter_val);
    assert (x_st->counter_val.lo % 2 == 1);
    blocking_xcb_sync_set_counter (x_st->xcb_c, x_st->counters[1], &x_st->counter_val);
}

void x11_notify_end_of_frame (struct x_state *x_st)
{
    increment_sync_counter (&x_st->counter_val);
    assert (x_st->counter_val.lo % 2 == 0);
    blocking_xcb_sync_set_counter (x_st->xcb_c, x_st->counters[1], &x_st->counter_val);
}

int main (void)
{
    // Setup clocks
    setup_clocks ();

    //////////////////
    // X11 setup
    // By default xcb is used, because it allows more granularity if we ever reach
    // performance issues, but for cases when we need Xlib functions we have an
    // Xlib Display too.
    struct x_state x_st;

    x_st.xlib_dpy = XOpenDisplay (NULL);
    if (!x_st.xlib_dpy) {
        printf ("Could not open display\n");
        return -1;
    }

    x_st.xcb_c = XGetXCBConnection (x_st.xlib_dpy);
    if (!x_st.xcb_c) {
        printf ("Could not get XCB x_st.xcb_c from Xlib Display\n");
        return -1;
    }
    XSetEventQueueOwner (x_st.xlib_dpy, XCBOwnsEventQueue);

    /* Get the first screen */
    // TODO: Get the default screen instead of assuming it's 0.
    // TODO: What happens if there is more than 1 screen?, probably will
    // have to iterate with xcb_setup_roots_iterator(), and xcb_screen_next ().
    x_st.screen = xcb_setup_roots_iterator (xcb_get_setup (x_st.xcb_c)).data;

    x11_create_window (&x_st, "Order Type");

    x11_setup_icccm_and_ewmh_protocols (&x_st);

    // Crerate backbuffer pixmap
    x_st.gc = xcb_generate_id (x_st.xcb_c);
    xcb_create_gc (x_st.xcb_c, x_st.gc, x_st.window, 0, NULL);
    x_st.backbuffer = xcb_generate_id (x_st.xcb_c);
    xcb_create_pixmap (x_st.xcb_c,
                       x_st.depth,                      /* depth of the screen */
                       x_st.backbuffer,                 /* id of the pixmap */
                       x_st.window,                     /* based on this drawable */
                       x_st.screen->width_in_pixels,    /* pixel width of the screen */
                       x_st.screen->height_in_pixels);  /* pixel height of the screen */

    // Create a cairo surface on backbuffer
    //
    // NOTE: Ideally we should be using cairo_xcb_* functions, but font
    // options are ignored because the patch that added this functionality was
    // disabled, so we are forced to use cairo_xlib_* functions.
    // TODO: Check the function _cairo_xcb_screen_get_font_options () in
    // <cairo>/src/cairo-xcb-screen.c and see if the issue has been resolved, if
    // it has, then go back to xcb for consistency with the rest of the code.
    // (As of git cffa452f44e it hasn't been solved).
    Visual *Xlib_visual = Visual_from_visualid (x_st.xlib_dpy, x_st.visual->visual_id);
    cairo_surface_t *surface =
        cairo_xlib_surface_create (x_st.xlib_dpy, x_st.backbuffer, Xlib_visual,
                                   x_st.screen->width_in_pixels, x_st.screen->height_in_pixels);
    cairo_t *cr = cairo_create (surface);

    xcb_map_window (x_st.xcb_c, x_st.window);
    xcb_flush (x_st.xcb_c);

    // PangoLayout for text handling
    PangoLayout *text_layout = pango_cairo_create_layout (cr);
    PangoFontDescription *font_desc = pango_font_description_from_string ("Open Sans 9");
    pango_layout_set_font_description (text_layout, font_desc);
    pango_font_description_free (font_desc);

    // ////////////////
    // Main event loop
    xcb_generic_event_t *event;
    app_graphics_t graphics;
    graphics.cr = cr;
    graphics.text_layout = text_layout;
    graphics.width = WINDOW_WIDTH;
    graphics.height = WINDOW_HEIGHT;
    bool force_blit = false;

    float frame_rate = 60;
    float target_frame_length_ms = 1000/(frame_rate);
    struct timespec start_ticks;
    struct timespec end_ticks;

    clock_gettime(CLOCK_MONOTONIC, &start_ticks);
    app_input_t app_input = {0};
    app_input.wheel = 1;

    mem_pool_t bootstrap = {0};
    struct app_state_t *st = mem_pool_push_size_full (&bootstrap, sizeof(struct app_state_t), POOL_ZERO_INIT);
    st->memory = bootstrap;

    while (!st->end_execution) {
        while ((event = xcb_poll_for_event (x_st.xcb_c))) {
            // NOTE: The most significant bit of event->response_type is set if
            // the event was generated from a SendEvent request, here we don't
            // care about the source of the event.
            switch (event->response_type & ~0x80) {
                case XCB_CONFIGURE_NOTIFY: {
                    graphics.width = ((xcb_configure_notify_event_t*)event)->width;
                    graphics.height = ((xcb_configure_notify_event_t*)event)->height;
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
                    force_blit = true;
                    break;
                case XCB_BUTTON_PRESS: {
                    char button_pressed = ((xcb_key_press_event_t*)event)->detail;
                    if (button_pressed == 4) {
                        app_input.wheel *= 1.2;
                    } else if (button_pressed == 5) {
                        app_input.wheel /= 1.2;
                    } else if (button_pressed >= 1 && button_pressed <= 3) {
                        app_input.mouse_down[button_pressed-1] = 1;
                    }
                    } break;
                case XCB_BUTTON_RELEASE: {
                    // NOTE: This loses clicks if the button press-release
                    // sequence happens in the same batch of events, right now
                    // it does not seem to be a problem.
                    char button_pressed = ((xcb_key_press_event_t*)event)->detail;
                    if (button_pressed >= 1 && button_pressed <= 3) {
                        app_input.mouse_down[button_pressed-1] = 0;
                    }
                    } break;
                case XCB_CLIENT_MESSAGE: {
                    bool handled = false;
                    xcb_client_message_event_t *client_message = ((xcb_client_message_event_t*)event);
                     
                    // WM_DELETE_WINDOW protocol
                    if (client_message->type == x_st.wm_protocols_a) {
                        if (client_message->data.data32[0] == x_st.delete_window_a) {
                            st->end_execution = true;
                        }
                        handled = true;
                    }

                    // _NET_WM_SYNC_REQUEST protocol using the extended mode
                    if (client_message->type == x_st.wm_protocols_a && client_message->data.data32[0] == x_st.net_wm_sync_a) {
                        x_st.counter_val.lo = client_message->data.data32[2];
                        x_st.counter_val.hi = client_message->data.data32[3];
                        if (x_st.counter_val.lo % 2 != 0) {
                            increment_sync_counter (&x_st.counter_val);
                        }
                        handled = true;
                    } else if (client_message->type == x_st.net_wm_frame_drawn_a) {
                        handled = true;
                    } else if (client_message->type == x_st.net_wm_frame_timings_a) {
                        handled = true;
                    }

                    if (!handled) {
                        printf ("Unrecognized Client Message: %s\n", get_x11_atom_name (x_st.xcb_c, client_message->type));
                    }
                    } break;
                case 0: { // XCB_ERROR
                    xcb_generic_error_t *error = (xcb_generic_error_t*)event;
                    printf("Received X11 error %d\n", error->error_code);
                    } break;
                default: 
                    /* Unknown event type, ignore it */
                    continue;
            }
            free (event);
        }

        x11_notify_start_of_frame (&x_st);

        // TODO: How bad is this? should we actually measure it?
        app_input.time_elapsed_ms = target_frame_length_ms;

        bool blit_needed = update_and_render (st, &graphics, app_input);

        cairo_status_t cr_stat = cairo_status (graphics.cr);
        if (cr_stat != CAIRO_STATUS_SUCCESS) {
            printf ("Cairo error: %s\n", cairo_status_to_string (cr_stat));
            return 0;
        }

        if (blit_needed || force_blit) {
            xcb_copy_area (x_st.xcb_c,
                           x_st.backbuffer,        /* drawable we want to paste */
                           x_st.window,            /* drawable on which we copy the previous Drawable */
                           x_st.gc,
                           0,0,0,0,
                           graphics.width,    /* pixel width of the region we want to copy */
                           graphics.height);  /* pixel height of the region we want to copy */
            force_blit = false;
        }

        x11_notify_end_of_frame (&x_st);

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

        clock_gettime(CLOCK_MONOTONIC, &end_ticks);
        //printf ("FPS: %f\n", 1000/time_elapsed_in_ms (&start_ticks, &end_ticks));
        start_ticks = end_ticks;

        xcb_flush (x_st.xcb_c);
        app_input.keycode = 0;
        app_input.wheel = 1;
        app_input.force_redraw = 0;
    }

    // These don't seem to free everything according to Valgrind, so we don't
    // care to free them, the process will end anyway.
    // cairo_surface_destroy(surface);
    // cairo_destroy (cr);
    // xcb_disconnect (x_st.xcb_c);

    return 0;
}

