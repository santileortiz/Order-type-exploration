// Dependencies:
// sudo apt-get install libxcb1-dev libcairo2-dev
#include <X11/Xlib-xcb.h>
#include <xcb/sync.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "gui.h"
#include "common.h"
#include "slo_timers.h"
#include "ot_db.h"
#include "geometry_combinatorics.h"
#include "app_api.h"
#include "tree_mode.h"
#include "grid_mode.h"
#include "point_set_mode.h"

// We do a unity build for now
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
        init_pressed_button (&st->css_styles[CSS_BUTTON_ACTIVE]);
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
                printf ("Double Click\n");
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
            printf ("Click\n");
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
    bool blit_needed;
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
    xcb_atom_t res;
    xcb_generic_error_t *err = NULL;
    xcb_intern_atom_cookie_t ck = xcb_intern_atom (c, 0, strlen(value), value);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error while requesting atom.\n");
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
        printf ("Error while requesting atom.\n");
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

int main (void)
{
    // Setup clocks
    setup_clocks ();

    //////////////////
    // X11 setup
    // By default xcb is used, because it allows more granularity if we ever reach
    // performance issues, but for cases when we need Xlib functions we have an
    // Xlib Display too.

    Display *dpy = XOpenDisplay (NULL);
    if (!dpy) {
        printf ("Could not open display\n");
        return -1;
    }

    xcb_connection_t *connection = XGetXCBConnection (dpy);
    if (!connection) {
        printf ("Could not get XCB connection from Xlib Display\n");
        return -1;
    }
    XSetEventQueueOwner (dpy, XCBOwnsEventQueue);

    /* Get the first screen */
    // TODO: Get the default screen instead of assuming it's 0.
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

    // Set WM_PROTOCOLS property
    xcb_atom_t delete_window_atom = get_x11_atom (connection, "WM_DELETE_WINDOW");
    xcb_atom_t net_wm_sync = get_x11_atom (connection, "_NET_WM_SYNC_REQUEST");
    xcb_atom_t atoms[] = {delete_window_atom, net_wm_sync};

    xcb_atom_t wm_protocols = get_x11_atom (connection, "WM_PROTOCOLS");
    xcb_change_property (connection,
            XCB_PROP_MODE_REPLACE,
            window,
            wm_protocols,
            XCB_ATOM_ATOM,
            32,
            ARRAY_SIZE(atoms),
            atoms);

    // Set up counters for _NET_WM_SYNC_REQUEST protocol on extended mode
    xcb_sync_int64_t counter_val = {0, 0}; //{HI, LO}
    xcb_gcontext_t counters[2];
    counters[0] = xcb_generate_id (connection);
    xcb_sync_create_counter (connection, counters[0], counter_val);

    counters[1] = xcb_generate_id (connection);
    xcb_sync_create_counter (connection, counters[1], counter_val);
    
    // NOTE: These atoms are used to recognize the ClientMessage type
    xcb_atom_t net_wm_frame_drawn = get_x11_atom (connection, "_NET_WM_FRAME_DRAWN");
    xcb_atom_t net_wm_frame_timings = get_x11_atom (connection, "_NET_WM_FRAME_TIMINGS");

    xcb_change_property (connection,
            XCB_PROP_MODE_REPLACE,
            window,
            get_x11_atom (connection, "_NET_WM_SYNC_REQUEST_COUNTER"),
            XCB_ATOM_CARDINAL,
            32,
            ARRAY_SIZE(counters),
            counters);

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

    // Create a cairo surface on window
    //
    // NOTE: Ideally we should be using cairo_xcb_* functions, but font
    // options are ignored because the patch that added this functionality was
    // disabled, so we are forced to use cairo_xlib_* functions.
    // TODO: Check the function _cairo_xcb_screen_get_font_options () in
    // <cairo>/src/cairo-xcb-screen.c and see if the issue has been resolved, if
    // it has, then go back to xcb for consistency with the rest of the code.
    // (As of git cffa452f44e it hasn't been solved).
    Visual *default_visual = DefaultVisual(dpy, DefaultScreen(dpy));
    cairo_surface_t *surface =
        cairo_xlib_surface_create (dpy, window, default_visual,
                                   WINDOW_WIDTH, WINDOW_HEIGHT);
    cairo_t *cr = cairo_create (surface);

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
    uint16_t pixmap_width = WINDOW_WIDTH;
    uint16_t pixmap_height = WINDOW_HEIGHT;
    bool make_pixmap_bigger = false;
    bool force_blit = false;

    float frame_rate = 60;
    float target_frame_length_ms = 1000/(frame_rate);
    struct timespec start_ticks;
    struct timespec end_ticks;

    clock_gettime(CLOCK_MONOTONIC, &start_ticks);
    app_input_t app_input = {0};
    app_input.wheel = 1;

    int storage_size =  megabyte(60);
    uint8_t* memory = calloc (storage_size, 1);
    struct app_state_t *st = (struct app_state_t*)memory;
    st->storage_size = storage_size;
    memory_stack_init (&st->memory, st->storage_size-sizeof(struct app_state_t), memory+sizeof(struct app_state_t));

    // TODO: Should this be inside st->memory?
    int temp_memory_size =  megabyte(10);
    uint8_t* temp_memory = calloc (temp_memory_size, 1);
    memory_stack_init (&st->temporary_memory, temp_memory_size, temp_memory);

    while (!st->end_execution) {
        while ((event = xcb_poll_for_event (connection))) {
            // NOTE: The most significant bit of event->response_type is set if
            // the event was generated from a SendEvent request, here we don't
            // care about the source of the event.
            switch (event->response_type & ~0x80) {
                case XCB_CONFIGURE_NOTIFY: {
                    uint16_t new_width = ((xcb_configure_notify_event_t*)event)->width;
                    uint16_t new_height = ((xcb_configure_notify_event_t*)event)->height;
                    if (new_width > pixmap_width || new_height > pixmap_height) {
                        make_pixmap_bigger = true;
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
                    if (client_message->type == wm_protocols) {
                        if (client_message->data.data32[0] == delete_window_atom) {
                            st->end_execution = true;
                        }
                        handled = true;
                    }

                    // _NET_WM_SYNC_REQUEST protocol using the extended mode
                    if (client_message->type == wm_protocols && client_message->data.data32[0] == net_wm_sync) {
                        counter_val.lo = client_message->data.data32[2];
                        counter_val.hi = client_message->data.data32[3];
                        handled = true;
                    } else if (client_message->type == net_wm_frame_drawn) {
                        handled = true;
                    } else if (client_message->type == net_wm_frame_timings) {
                        handled = true;
                    }

                    if (!handled) {
                        printf ("Unrecognized Client Message: %s\n", get_x11_atom_name (connection, client_message->type));
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

        // Notify X11: Start of frame
        increment_sync_counter (&counter_val);
        assert (counter_val.lo % 2 == 1);
        xcb_sync_set_counter (connection, counters[1], counter_val);

        if (make_pixmap_bigger) {
            pixmap_width = graphics.width;
            pixmap_height = graphics.height;
            xcb_free_pixmap (connection, backbuffer);
            backbuffer = xcb_generate_id (connection);
            xcb_create_pixmap (connection, screen->root_depth, backbuffer, window,
                    pixmap_width, pixmap_height);
            cairo_xlib_surface_set_drawable (cairo_get_target (graphics.cr), backbuffer,
                    pixmap_width, pixmap_height);
            app_input.force_redraw = true;
            make_pixmap_bigger = false;
        }

        // TODO: How bad is this? should we actually measure it?
        app_input.time_elapsed_ms = target_frame_length_ms;

        bool blit_needed = update_and_render (st, &graphics, app_input);

        // Notify X11: End of frame
        increment_sync_counter (&counter_val);
        assert (counter_val.lo % 2 == 0);
        xcb_sync_set_counter (connection, counters[1], counter_val);

        cairo_status_t cr_stat = cairo_status (graphics.cr);
        if (cr_stat != CAIRO_STATUS_SUCCESS) {
            printf ("Cairo error: %s\n", cairo_status_to_string (cr_stat));
            return 0;
        }

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

        if (blit_needed || force_blit) {
            xcb_copy_area (connection,
                    backbuffer,  /* drawable we want to paste */
                    window, /* drawable on which we copy the previous Drawable */
                    gc,
                    0,0,0,0,
                    graphics.width,         /* pixel width of the region we want to copy */
                    graphics.height);      /* pixel height of the region we want to copy */
            force_blit = false;
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

