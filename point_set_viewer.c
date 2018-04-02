/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#include <X11/Xlib-xcb.h>
#include <xcb/sync.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>
#include <pthread.h>

#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
//#define NDEBUG
#include <assert.h>
#include <errno.h>

#define HTTP_IMPLEMENTATION
#include "libs/http.h"
#include "common.h"
#include "gui.h"
#include "slo_timers.h"
#include "order_types.h"

#include "geometry_combinatorics.h"
#include "tree_mode.h"
#include "grid_mode.h"
#include "point_set_mode.h"
#include "config.c"
#include "app_api.h"

#define SEQUENCE_STORE_IMPL
#include "sequence_store.h"

// We do a unity build for now
#include "order_types.c"
#include "tree_mode.c"
#include "grid_mode.c"
#include "point_set_mode.c"
#include "download_screen.c"

#define WINDOW_HEIGHT 700
#define WINDOW_WIDTH 700

bool update_and_render (struct app_state_t *st, app_graphics_t *graphics, app_input_t input)
{
    st->gui_st.gr = *graphics;
    if (!st->is_initialized) {
        st->end_execution = false;
        st->is_initialized = true;

        if (!ensure_dir_exists ("~/.ps_viewer")) {
            st->end_execution = true;
            printf ("Couldn't create ~/.ps_viewer/ directory for configuration files.");
        }

        default_gui_init (&st->gui_st);
        global_gui_st = &st->gui_st;
        st->temporary_memory_flush = mem_pool_begin_temporary_memory (&st->temporary_memory);

        st->download_database = false;
        int missing_files[10], num_missing_files;
        check_database (st->config->database_location, missing_files, &num_missing_files);

        int to_download[10], num_to_download = 0;
        int i;
        for (i=0; i<num_missing_files; i++) {
            if (in_array (missing_files[i], st->config->n_values, st->config->num_n_values)) {
                to_download[num_to_download] = missing_files[i];
                num_to_download++;
            }
        }

        if (num_to_download != 0) {
            st->download_database = true;

            mem_pool_t bootstrap_mem = {0};
            st->dl_st = mem_pool_push_size_full (&bootstrap_mem,
                                                 sizeof(struct database_download_t),
                                                 POOL_ZERO_INIT);
            st->dl_st->mem = bootstrap_mem;
            st->dl_st->num_to_download = num_to_download;
            int i;
            for (i=0; i<num_to_download; i++) {
                st->dl_st->to_download[i] = to_download[i];
            }
        }
    }

    bool blit_needed = false;
    // TODO: If we ever allow more download options then make this a mode, not a
    // one-time thing.
    if (st->download_database) {
        blit_needed = download_database_screen (st, graphics);

    } else {
        mem_pool_end_temporary_memory (st->temporary_memory_flush);

        update_input (&st->gui_st, input);

        switch (st->gui_st.input.keycode) {
            case 24: //KEY_Q
                st->end_execution = true;
                // NOTE: We want modes to be called once after we end execution
                // so they can do their own cleanup.
                break;
            default:
                //if (input.keycode >= 8) {
                //    printf ("%" PRIu8 "\n", input.keycode);
                //    //printf ("%" PRIu16 "\n", input.modifiers);
                //}
                break;
        }

#ifdef RELEASE_BUILD
        blit_needed = point_set_mode (st, graphics);
#else
        if (st->gui_st.input.keycode == 58) { //KEY_M
            st->app_mode = (st->app_mode + 1)%NUM_APP_MODES;
            st->gui_st.input.force_redraw = true;
        }

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
#endif
    }

    cairo_surface_flush (cairo_get_target(graphics->cr));
    return blit_needed;
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

    xcb_timestamp_t last_timestamp; // Last server time received in an event.
    xcb_sync_counter_t counters[2];
    xcb_sync_int64_t counter_val;
    mem_pool_temp_marker_t transient_pool_flush;
    mem_pool_t transient_pool;

    xcb_timestamp_t clipboard_ownership_timestamp;
    bool have_clipboard_ownership;
};

struct x_state global_x11_state;

xcb_atom_t get_x11_atom (xcb_connection_t *c, const char *value)
{
    xcb_atom_t res = XCB_ATOM_NONE;
    xcb_generic_error_t *err = NULL;
    xcb_intern_atom_cookie_t ck = xcb_intern_atom (c, 0, strlen(value), value);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error while requesting atom.\n");
        free (err);
        return res;
    }
    res = reply->atom;
    free(reply);
    return res;
}

enum cached_atom_names_t {
    // WM_DELETE_WINDOW protocol
    LOC_ATOM_WM_DELETE_WINDOW,

    // _NET_WM_SYNC_REQUEST protocol
    LOC_ATOM__NET_WM_SYNC_REQUEST,
    LOC_ATOM__NET_WM_SYNC__REQUEST_COUNTER,
    LOC_ATOM__NET_WM_SYNC,
    LOC_ATOM__NET_WM_FRAME_DRAWN,
    LOC_ATOM__NET_WM_FRAME_TIMINGS,

    LOC_ATOM_WM_PROTOCOLS,

    // Related to clipboard management
    LOC_ATOM_CLIPBOARD,
    LOC_ATOM__CLIPBOARD_CONTENT,
    LOC_ATOM_TARGETS,
    LOC_ATOM_TIMESTAMP,
    LOC_ATOM_MULTIPLE,
    LOC_ATOM_UTF8_STRING,
    LOC_ATOM_TEXT,
    LOC_ATOM_TEXT_MIME,
    LOC_ATOM_TEXT_MIME_CHARSET,
    LOC_ATOM_ATOM_PAIR,

    NUM_ATOMS_CACHE
};                       

xcb_atom_t xcb_atoms_cache[NUM_ATOMS_CACHE];

struct atom_enum_name_t {
    enum cached_atom_names_t id;
    char *name;
};

void init_x11_atoms (struct x_state *x_st)
{
    struct atom_enum_name_t atom_arr[] = {
        {LOC_ATOM_WM_DELETE_WINDOW, "WM_DELETE_WINDOW"},
        {LOC_ATOM__NET_WM_SYNC_REQUEST, "_NET_WM_SYNC_REQUEST"},
        {LOC_ATOM__NET_WM_SYNC__REQUEST_COUNTER, "_NET_WM_SYNC__REQUEST_COUNTER"},
        {LOC_ATOM__NET_WM_SYNC, "_NET_WM_SYNC"},
        {LOC_ATOM__NET_WM_FRAME_DRAWN, "_NET_WM_FRAME_DRAWN"},
        {LOC_ATOM__NET_WM_FRAME_TIMINGS, "_NET_WM_FRAME_TIMINGS"},
        {LOC_ATOM_WM_PROTOCOLS, "WM_PROTOCOLS"},
        {LOC_ATOM_CLIPBOARD, "CLIPBOARD"},
        {LOC_ATOM__CLIPBOARD_CONTENT, "_CLIPBOARD_CONTENT"},
        {LOC_ATOM_TARGETS, "TARGETS"},
        {LOC_ATOM_TIMESTAMP, "TIMESTAMP"},
        {LOC_ATOM_MULTIPLE, "MULTIPLE"},
        {LOC_ATOM_UTF8_STRING, "UTF8_STRING"},
        {LOC_ATOM_TEXT, "TEXT"},
        {LOC_ATOM_TEXT_MIME, "text/plain"},
        {LOC_ATOM_TEXT_MIME_CHARSET, "text/plain;charset=utf-8"},
        {LOC_ATOM_ATOM_PAIR, "ATOM_PAIR"}
    };

    xcb_intern_atom_cookie_t cookies[ARRAY_SIZE(atom_arr)];

    int i;
    for (i=0; i<ARRAY_SIZE(atom_arr); i++) {
        char *name = atom_arr[i].name;
        cookies[i] = xcb_intern_atom (x_st->xcb_c, 0, strlen(name), name);
    }

    for (i=0; i<ARRAY_SIZE(atom_arr); i++) {
        xcb_generic_error_t *err = NULL;
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (x_st->xcb_c, cookies[i], &err);
        if (err != NULL) {
            printf ("Error while requesting atom in batch.\n");
            free (err);
            continue;
        }
        xcb_atoms_cache[atom_arr[i].id] = reply->atom;
        free (reply);
    }
}

char* get_x11_atom_name (xcb_connection_t *c, xcb_atom_t atom, mem_pool_t *pool)
{
    if (atom == XCB_ATOM_NONE) {
        return "NONE";
    }

    xcb_get_atom_name_cookie_t ck = xcb_get_atom_name (c, atom);
    xcb_generic_error_t *err = NULL;
    xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error while requesting atom's name.\n");
        free (err);
    }

    char *name = xcb_get_atom_name_name (reply);
    size_t len = xcb_get_atom_name_name_length (reply);

    char *res = mem_pool_push_size (pool, len+1);
    memcpy (res, name, len);
    res[len] = '\0';

    return res;
}

void get_x11_property_part (xcb_connection_t *c, xcb_drawable_t window,
                            xcb_atom_t property, uint32_t offset, uint32_t len,
                            xcb_get_property_reply_t **reply)
{
    xcb_get_property_cookie_t ck =
        xcb_get_property (c, 0, window, property, XCB_GET_PROPERTY_TYPE_ANY,
                          offset, len);
    xcb_generic_error_t *err = NULL;
    *reply = xcb_get_property_reply (c, ck, &err);
    if (err != NULL) {
        printf ("Error reading property.\n");
        free (err);
    }
}

char* get_x11_text_property (xcb_connection_t *c, mem_pool_t *pool,
                             xcb_drawable_t window, xcb_atom_t property,
                             size_t *len)
{

    xcb_get_property_reply_t *reply_1, *reply_2;
    uint32_t first_request_size = 10;
    get_x11_property_part (c, window, property, 0, first_request_size, &reply_1);

    uint32_t len_1, len_total;
    len_1 = len_total = xcb_get_property_value_length (reply_1);

    if (reply_1->type == XCB_ATOM_NONE) {
        *len = 0;
        return NULL;
    }

    if (reply_1->type != xcb_atoms_cache[LOC_ATOM_UTF8_STRING] &&
        reply_1->type != XCB_ATOM_STRING &&
        reply_1->type != xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET] &&
        reply_1->type != xcb_atoms_cache[LOC_ATOM_TEXT_MIME]) {
        mem_pool_t temp_pool = {0};
        printf ("Invalid text property (%s)\n", get_x11_atom_name (c, reply_1->type, &temp_pool));
        mem_pool_destroy (&temp_pool);
        return NULL;
    }

    if (reply_1->bytes_after != 0) {
        get_x11_property_part (c, window, property, first_request_size,
                               I_CEIL_DIVIDE(reply_1->bytes_after,4), &reply_2);
        len_total += xcb_get_property_value_length (reply_2);
    }

    char *res = pom_push_size (pool, len_total+1);

    memcpy (res, xcb_get_property_value (reply_1), len_1);
    free (reply_1);
    if (len_total - len_1) {
        memcpy ((char*)res+len_1, xcb_get_property_value (reply_2), len_total-len_1);
        free (reply_2);
    }
    res[len_total] = '\0';

    if (len != NULL) {
        *len = len_total;
    }

    return res;
}

void* get_x11_property (xcb_connection_t *c, mem_pool_t *pool, xcb_drawable_t window,
                        xcb_atom_t property, size_t *len, xcb_atom_t *type)
{

    xcb_get_property_reply_t *reply_1, *reply_2;
    uint32_t first_request_size = 10;
    get_x11_property_part (c, window, property, 0, first_request_size, &reply_1);
    uint32_t len_1 = *len = xcb_get_property_value_length (reply_1);
    if (reply_1->bytes_after != 0) {
        get_x11_property_part (c, window, property, first_request_size,
                               I_CEIL_DIVIDE(reply_1->bytes_after,4), &reply_2);
        *len += xcb_get_property_value_length (reply_2);
    }

    void *res = mem_pool_push_size (pool, *len);
    *type = reply_1->type;

    memcpy (res, xcb_get_property_value (reply_1), len_1);
    free (reply_1);
    if (*len - len_1) {
        memcpy ((char*)res+len_1, xcb_get_property_value (reply_2), *len-len_1);
        free (reply_2);
    }

    return res;
}

void print_x11_property (xcb_connection_t *c, xcb_drawable_t window, xcb_atom_t property)
{
    mem_pool_t pool = {0};
    size_t len;
    xcb_atom_t type;

    if (property == XCB_ATOM_NONE) {
        printf ("NONE\n");
        return;
    }

    void * value = get_x11_property (c, &pool, window, property, &len, &type);
    printf ("%s (%s)", get_x11_atom_name(c, property, &pool), get_x11_atom_name(c, type, &pool));
    if (type == xcb_atoms_cache[LOC_ATOM_UTF8_STRING] ||
        type == xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET] ||
        type == xcb_atoms_cache[LOC_ATOM_TEXT_MIME] ||
        type == XCB_ATOM_STRING) {

        if (type == XCB_ATOM_STRING) {
            // NOTE: This is a latin1 encoded string so some characters won't
            // print niceley, we also show the binary data so that people notice
            // this. Still it's better to show some string than just the binary
            // data.
            printf (" = %.*s (", (int)len, (char*)value);
            unsigned char *ptr = (unsigned char*)value;
            len--;
            while (len) {
                printf ("0x%X ", *ptr);
                ptr++;
                len--;
            }
            printf ("0x%X)\n", *ptr);
        } else {
            printf (" = %.*s\n", (int)len, (char*)value);
        }
    } else if (type == XCB_ATOM_ATOM) {
        xcb_atom_t *atom_list = (xcb_atom_t*)value;
        printf (" = ");
        while (len > sizeof(xcb_atom_t)) {
            printf ("%s, ", get_x11_atom_name (c, *atom_list, &pool));
            atom_list++;
            len -= sizeof(xcb_atom_t);
        }
        printf ("%s\n", get_x11_atom_name (c, *atom_list, &pool));
    } else if (type == XCB_ATOM_NONE) {
        printf ("\n");
    } else {
        unsigned char *ptr = (unsigned char*)value;
        if (len != 0) {
            printf (" = ");
            len--;
            while (len) {
                printf ("0x%X ", *ptr);
                ptr++;
                len--;
            }
            printf ("0x%X\n", *ptr);
        } else {
            printf ("\n");
        }
    }
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
    Visual *retval = found->visual;
    XFree (found);
    return retval;
}

void x11_change_property (xcb_connection_t *c, xcb_drawable_t window,
                          xcb_atom_t property, xcb_atom_t type,
                          uint32_t len, void *data,
                          bool checked)
{
    uint8_t format = 32;
    if (type == XCB_ATOM_STRING ||
        type == xcb_atoms_cache[LOC_ATOM_UTF8_STRING] ||
        type == xcb_atoms_cache[LOC_ATOM_TEXT_MIME] ||
        type == xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET])
    {
        format = 8;
    } else if (type == XCB_ATOM_ATOM ||
               type == XCB_ATOM_CARDINAL ||
               type == XCB_ATOM_INTEGER) {
        format = 32;
    }

    if (checked) {
        xcb_void_cookie_t ck =
            xcb_change_property_checked (c,
                                         XCB_PROP_MODE_REPLACE,
                                         window,
                                         property,
                                         type,
                                         format,
                                         len,
                                         data);

        xcb_generic_error_t *error;
        if ((error = xcb_request_check(c, ck))) {
            printf("Error changing property %d\n", error->error_code);
            free(error);
        }
    } else {
        xcb_change_property (c,
                             XCB_PROP_MODE_REPLACE,
                             window,
                             property,
                             type,
                             format,
                             len,
                             data);
    }
}

void x11_create_window (struct x_state *x_st, char *title)
{
    uint32_t event_mask = XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|
                             XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_BUTTON_PRESS|
                             XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION|
                             XCB_EVENT_MASK_PROPERTY_CHANGE;
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
    x11_change_property (x_st->xcb_c,
            x_st->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            strlen (title),
            title,
            false);
}

void x11_setup_icccm_and_ewmh_protocols (struct x_state *x_st, char *arg0)
{
    xcb_atom_t net_wm_protocols_value[2];

    // Set up counters for extended mode
    x_st->counter_val = (xcb_sync_int64_t){0, 0}; //{HI, LO}
    x_st->counters[0] = xcb_generate_id (x_st->xcb_c);
    xcb_sync_create_counter (x_st->xcb_c, x_st->counters[0], x_st->counter_val);

    x_st->counters[1] = xcb_generate_id (x_st->xcb_c);
    xcb_sync_create_counter (x_st->xcb_c, x_st->counters[1], x_st->counter_val);

    x11_change_property (x_st->xcb_c,
                         x_st->window,
                         xcb_atoms_cache[LOC_ATOM__NET_WM_SYNC__REQUEST_COUNTER],
                         XCB_ATOM_CARDINAL,
                         ARRAY_SIZE(x_st->counters),
                         x_st->counters,
                         false);

    // Set WM_PROTOCOLS property
    net_wm_protocols_value[0] = xcb_atoms_cache[LOC_ATOM_WM_DELETE_WINDOW];
    net_wm_protocols_value[1] = xcb_atoms_cache[LOC_ATOM__NET_WM_SYNC_REQUEST];

    x11_change_property (x_st->xcb_c,
                         x_st->window,
                         xcb_atoms_cache[LOC_ATOM_WM_PROTOCOLS],
                         XCB_ATOM_ATOM,
                         ARRAY_SIZE(net_wm_protocols_value),
                         net_wm_protocols_value,
                         false);

    // Set window WM_CLASS to allow other applications track resources
    //
    // TODO: Some people may expect to set the instance name using the
    // --name <instance> option or with the RESOURCE_NAME environment variable.
    int len = strlen (arg0);
    char *c = &arg0[len];
    while (c > arg0 && *(c-1) != '/') {
        c--;
    }

    string_t instance = str_new (c);
    int end = str_len(&instance)+1;
    str_put_c (&instance, end, c);
    // This capitalization won't work if the binary's name is not ASCII but I've
    // never seen that before.
    str_data(&instance)[end] &= ~0x20;

    x11_change_property (x_st->xcb_c,
            x_st->window,
            XCB_ATOM_WM_CLASS,
            XCB_ATOM_STRING,
            str_len(&instance),
            str_data(&instance),
            false);
    str_free (&instance);
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

void x11_print_window_name (struct x_state *x_st, xcb_drawable_t window)
{
    size_t len;
    char *prop = get_x11_text_property (x_st->xcb_c, &x_st->transient_pool, window, XCB_ATOM_WM_NAME, &len);
    printf ("id: 0x%x, \"%*s\"\n", window, (uint32_t)len, prop);
}

// NOTE: IMPORTANT!!! event MUST have 32 bytes allocated, otherwise we will read
// invalid values. Use xcb_generic_event_t and NOT xcb_<some_event>_event_t.
void x11_send_event (xcb_connection_t *c, xcb_drawable_t window, void *event)
{
    xcb_void_cookie_t ck = xcb_send_event_checked (c, 0, window, 0, event);
    xcb_generic_error_t *error;
    if ((error = xcb_request_check(c, ck))) {
        printf("Error sending event. %d\n", error->error_code);
        free(error);
    }
}

////////////////////////////////////////////////////
// Code to handle selections (copy and paste) in X11

//#define DEBUG_X11_SELECTIONS
void print_selection_request_event (struct x_state *x_st,
                                    xcb_selection_request_event_t *selection_request_ev)
{
    printf ("Time: %u\n", selection_request_ev->time);
    printf ("Owner window: ");
    x11_print_window_name (x_st, selection_request_ev->owner);
    printf ("Requestor window: ");
    x11_print_window_name (x_st, selection_request_ev->requestor);
    printf ("Selection: %s\n",
            get_x11_atom_name(x_st->xcb_c, selection_request_ev->selection, &x_st->transient_pool));
    printf ("Target: %s\n",
            get_x11_atom_name(x_st->xcb_c, selection_request_ev->target, &x_st->transient_pool));
    printf ("Property:\n");
    print_x11_property (x_st->xcb_c, selection_request_ev->requestor, selection_request_ev->property);
}

void print_selection_notify_event (struct x_state *x_st,
                                   xcb_selection_notify_event_t *selection_notify_ev)
{
    printf ("Time: %u\n", selection_notify_ev->time);
    printf ("Requestor window: ");
    x11_print_window_name (x_st, selection_notify_ev->requestor);
    printf ("Selection: %s\n",
            get_x11_atom_name(x_st->xcb_c, selection_notify_ev->selection, &x_st->transient_pool));
    printf ("Target: %s\n",
            get_x11_atom_name(x_st->xcb_c, selection_notify_ev->target, &x_st->transient_pool));
    printf ("Property:\n");
    print_x11_property (x_st->xcb_c, x_st->window, selection_notify_ev->property);
}

void x11_own_selection (struct x_state *x_st)
{
    xcb_atom_t clipboard_atom = xcb_atoms_cache[LOC_ATOM_CLIPBOARD];
    xcb_set_selection_owner (x_st->xcb_c, x_st->window, clipboard_atom, x_st->last_timestamp);
    xcb_get_selection_owner_cookie_t ck = xcb_get_selection_owner (x_st->xcb_c, clipboard_atom);
    xcb_generic_error_t *err = NULL;
    xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply (x_st->xcb_c, ck, &err);
    if (err != NULL) {
        printf ("Clipboard selection ownership acquisition failed.\n");
        free (err);
    }

    if (reply->owner != x_st->window) {
        x11_print_window_name (x_st, reply->owner);
        printf ("I'm not the owner.\n");
        x_st->have_clipboard_ownership = false;
    } else {
        x_st->have_clipboard_ownership = true;
    }

    free (reply);
}

PLATFORM_SET_CLIPBOARD_STR (x11_set_clipboard)
{
    struct x_state *x_st = &global_x11_state;
    if (!x_st->have_clipboard_ownership) {
        x11_own_selection (x_st);
    }
    free (global_gui_st->clipboard_str);
    global_gui_st->clipboard_str = strndup (str, len);
}

void convert_selection_request (struct x_state *x_st, const char *target)
{
    xcb_atom_t clipboard_atom = xcb_atoms_cache[LOC_ATOM_CLIPBOARD];
    xcb_atom_t target_atom = get_x11_atom (x_st->xcb_c, target);
    xcb_atom_t my_selection_atom = xcb_atoms_cache[LOC_ATOM__CLIPBOARD_CONTENT];
    xcb_convert_selection (x_st->xcb_c,
                           x_st->window,
                           clipboard_atom,
                           target_atom,
                           my_selection_atom,
                           XCB_CURRENT_TIME);

#ifdef DEBUG_X11_SELECTIONS
    printf ("\n------CONVERT SELECTION REQUEST [SENT]---------\n");
    printf ("Time: %li\n", XCB_CURRENT_TIME);
    printf ("Requestor window: ");
    x11_print_window_name (x_st, x_st->window);
    printf ("Selection: %s\n",
            get_x11_atom_name(x_st->xcb_c, clipboard_atom, &x_st->transient_pool));
    printf ("Target: %s\n", target);
    printf ("Property:\n");
    print_x11_property (x_st->xcb_c, x_st->window, my_selection_atom);
#endif
}

void convert_selection_request_atom (struct x_state *x_st, xcb_atom_t target)
{
    xcb_atom_t clipboard_atom = xcb_atoms_cache[LOC_ATOM_CLIPBOARD];
    xcb_atom_t my_selection_atom = xcb_atoms_cache[LOC_ATOM__CLIPBOARD_CONTENT];
    xcb_convert_selection (x_st->xcb_c,
                           x_st->window,
                           clipboard_atom,
                           target,
                           my_selection_atom,
                           XCB_CURRENT_TIME);

#ifdef DEBUG_X11_SELECTIONS
    printf ("\n------CONVERT SELECTION REQUEST [SENT]---------\n");
    printf ("Time: %li\n", XCB_CURRENT_TIME);
    printf ("Requestor window: ");
    x11_print_window_name (x_st, x_st->window);
    printf ("Selection: %s\n",
            get_x11_atom_name(x_st->xcb_c, clipboard_atom, &x_st->transient_pool));
    printf ("Target: %s\n",
            get_x11_atom_name(x_st->xcb_c, target, &x_st->transient_pool));
    printf ("Property:\n");
    print_x11_property (x_st->xcb_c, x_st->window, my_selection_atom);
#endif
}

PLATFORM_GET_CLIPBOARD_STR (x11_get_clipboard)
{
    struct gui_state_t *gui_st = global_gui_st;
    if (!global_x11_state.have_clipboard_ownership) {
        convert_selection_request_atom (&global_x11_state, xcb_atoms_cache[LOC_ATOM_TARGETS]);
        gui_st->clipboard_ready = false;
    } else {
        gui_st->clipboard_ready = true;
    }
}

void handle_selection_request_event (struct x_state *x_st,
                                     xcb_selection_request_event_t *selection_request_ev,
                                     xcb_selection_notify_event_t *reply)
{
    xcb_atom_t target = selection_request_ev->target;
    if (target == xcb_atoms_cache[LOC_ATOM_TARGETS]) {
        xcb_atom_t targets[] = {xcb_atoms_cache[LOC_ATOM_TIMESTAMP],
                                xcb_atoms_cache[LOC_ATOM_TARGETS],
                                xcb_atoms_cache[LOC_ATOM_MULTIPLE],
                                xcb_atoms_cache[LOC_ATOM_UTF8_STRING],
                                xcb_atoms_cache[LOC_ATOM_TEXT],
                                xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET]};

        x11_change_property (x_st->xcb_c,
                             selection_request_ev->requestor,
                             selection_request_ev->property,
                             XCB_ATOM_ATOM,
                             ARRAY_SIZE(targets),
                             targets,
                             true);

    } else if (target == xcb_atoms_cache[LOC_ATOM_TIMESTAMP]) {
        x11_change_property (x_st->xcb_c,
                             selection_request_ev->requestor,
                             selection_request_ev->property,
                             XCB_ATOM_INTEGER,
                             1,
                             &x_st->clipboard_ownership_timestamp,
                             false);

    } else if (target == xcb_atoms_cache[LOC_ATOM_UTF8_STRING] ||
               target == xcb_atoms_cache[LOC_ATOM_TEXT] ||
               target == xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET]) {
        xcb_atom_t type;
        if (selection_request_ev->target == xcb_atoms_cache[LOC_ATOM_TEXT]) {
            type = xcb_atoms_cache[LOC_ATOM_UTF8_STRING];
        } else {
            type = selection_request_ev->target;
        }

        uint32_t len = 0;
        if (global_gui_st->clipboard_str != NULL) {
            len = strlen(global_gui_st->clipboard_str);
        }

        x11_change_property (x_st->xcb_c,
                             selection_request_ev->requestor,
                             selection_request_ev->property,
                             type,
                             len,
                             global_gui_st->clipboard_str,
                             false);

    } else if (target == xcb_atoms_cache[LOC_ATOM_MULTIPLE]) {
        bool refuse_request = false;
        if (selection_request_ev->property != XCB_ATOM_NONE) {
            size_t len;
            xcb_atom_t type;
            void * value =
                get_x11_property (x_st->xcb_c, &x_st->transient_pool,
                                  selection_request_ev->requestor,
                                  selection_request_ev->property,
                                  &len, &type);
            if (type == xcb_atoms_cache[LOC_ATOM_ATOM_PAIR]) {
                xcb_atom_t *atom_list = (xcb_atom_t*)value;
                xcb_selection_request_event_t dummy_ev = *selection_request_ev;

                if (len % 2*sizeof(xcb_atom_t) == 0) {
                    while (len) {
                        if (*atom_list == xcb_atoms_cache[LOC_ATOM_MULTIPLE]) {
                            // NOTE: This would cause infinite recursion.
                            refuse_request = true;
                            break;
                        } else {
                            dummy_ev.target = *atom_list;
                        }
                        atom_list++;

                        if (*atom_list == XCB_ATOM_NONE) {
                            refuse_request = true;
                            break;
                        } else {
                            dummy_ev.property = *atom_list;
                        }
                        atom_list++;

                        handle_selection_request_event (x_st, &dummy_ev, NULL);
                        atom_list += 2;
                        len -= 2*sizeof(xcb_atom_t);
                    }
                } else {
                    refuse_request = true;
                }
            }
        } else {
            refuse_request = true;
        }

        if (refuse_request) {
            selection_request_ev->property = XCB_ATOM_NONE;
        }
    } else {
        // NOTE: Unknown target.
        selection_request_ev->property = XCB_ATOM_NONE;
    }

    if (reply != NULL) {
        *reply = (xcb_selection_notify_event_t){0};
        reply->response_type = XCB_SELECTION_NOTIFY;
        reply->time = selection_request_ev->time;
        reply->requestor = selection_request_ev->requestor;
        reply->selection = selection_request_ev->selection;
        reply->target = selection_request_ev->target;
        reply->property = selection_request_ev->property;
    }
}

void x11_handle_selections (struct x_state *x_st, struct gui_state_t *gui_st, xcb_generic_event_t *event)
{
    switch (event->response_type & ~0x80) {
        case XCB_SELECTION_REQUEST:
            {
                xcb_selection_request_event_t *selection_request_ev =
                    (xcb_selection_request_event_t*)event;

#ifdef DEBUG_X11_SELECTIONS
                printf ("\n----SELECTION REQUEST EVENT [RECEIVED]-----\n");
                print_selection_request_event (x_st, selection_request_ev);
#endif

                xcb_generic_event_t evt = {0};
                xcb_selection_notify_event_t *reply = (xcb_selection_notify_event_t*)&evt;
                handle_selection_request_event (x_st, selection_request_ev, reply);

                x11_send_event (x_st->xcb_c, selection_request_ev->requestor, reply);

#ifdef DEBUG_X11_SELECTIONS
                printf ("\n------SELECTION NOTIFY EVENT [SENT]------\n");
                print_selection_notify_event (x_st, reply);
#endif
            } break;
        case XCB_SELECTION_NOTIFY:
            {
                xcb_selection_notify_event_t *selection_notify_ev =
                    (xcb_selection_notify_event_t*)event;

#ifdef DEBUG_X11_SELECTIONS
                printf ("\n------SELECTION NOTIFY EVENT [RECEIVED]------\n");
                print_selection_notify_event (x_st, selection_notify_ev);
#endif

                if (selection_notify_ev->property == XCB_ATOM_NONE) {
                    break;
                }

                if (selection_notify_ev->target == XCB_ATOM_STRING) {
                    // TODO: We should convert from latin1 to UTF8 here.
                    printf ("Best target match for selection owner is STRING"
                            " but we don't support conversion from latin1 to"
                            " utf8 yet!\n");

                } else if (selection_notify_ev->target == xcb_atoms_cache[LOC_ATOM_UTF8_STRING] ||
                           selection_notify_ev->target == xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET] ||
                           selection_notify_ev->target == xcb_atoms_cache[LOC_ATOM_TEXT_MIME]) {
                    free (gui_st->clipboard_str);
                    gui_st->clipboard_str =
                        get_x11_text_property (x_st->xcb_c, NULL,
                                               x_st->window,
                                               selection_notify_ev->property,NULL);
                    gui_st->clipboard_ready = true;

                } else if (selection_notify_ev->target == xcb_atoms_cache[LOC_ATOM_TARGETS]) {
                    bool invalid_response = false;
                    size_t len;
                    xcb_atom_t type;
                    void * value = get_x11_property (x_st->xcb_c,
                                                     &x_st->transient_pool,
                                                     x_st->window,
                                                     selection_notify_ev->property,
                                                     &len, &type);

                    xcb_atom_t supported_targets[] = {xcb_atoms_cache[LOC_ATOM_UTF8_STRING],
                                                      xcb_atoms_cache[LOC_ATOM_TEXT_MIME_CHARSET],
                                                      xcb_atoms_cache[LOC_ATOM_TEXT_MIME], // ASCII
                                                      XCB_ATOM_STRING}; // latin1
                    int best_match = ARRAY_SIZE(supported_targets);

                    if (type == XCB_ATOM_ATOM) {
                        xcb_atom_t *atom_list = (xcb_atom_t*)value;

                        if (len % sizeof(xcb_atom_t)) {invalid_response = true;}
                        while (len) {
                            int i;
                            for (i=0; i< ARRAY_SIZE(supported_targets); i++) {
                                if (*atom_list == supported_targets[i] && i<best_match) {
                                    best_match = i;
                                    break;
                                }
                            }

                            atom_list++;
                            len -= sizeof(xcb_atom_t);
                        }
                    } else {
                        invalid_response = true;
                    }

                    if (!invalid_response) {
                        convert_selection_request_atom (x_st, supported_targets[best_match]);
                    } else {
                        printf ("Invalid response to TARGETS selection conversion.\n");
                    }
                }
            } break;
        case XCB_SELECTION_CLEAR:
            {
                x_st->have_clipboard_ownership = false;
                free (gui_st->clipboard_str);
                gui_st->clipboard_str = NULL;
                gui_st->clipboard_ready = false;
            } break;
    }
}

int main (int argc, char ** argv)
{
    // Setup clocks
    setup_clocks ();

    //////////////////
    // X11 setup
    // By default xcb is used, because it allows more granularity if we ever reach
    // performance issues, but for cases when we need Xlib functions we have an
    // Xlib Display too.
    struct x_state *x_st = &global_x11_state;
    x_st->transient_pool_flush = mem_pool_begin_temporary_memory (&x_st->transient_pool);

    x_st->xlib_dpy = XOpenDisplay (NULL);
    if (!x_st->xlib_dpy) {
        printf ("Could not open display\n");
        return -1;
    }

    x_st->xcb_c = XGetXCBConnection (x_st->xlib_dpy);
    if (!x_st->xcb_c) {
        printf ("Could not get XCB x_st->xcb_c from Xlib Display\n");
        return -1;
    }
    XSetEventQueueOwner (x_st->xlib_dpy, XCBOwnsEventQueue);

    init_x11_atoms (x_st);

    /* Get the first screen */
    // TODO: Get the default screen instead of assuming it's 0.
    // TODO: What happens if there is more than 1 screen?, probably will
    // have to iterate with xcb_setup_roots_iterator(), and xcb_screen_next ().
    x_st->screen = xcb_setup_roots_iterator (xcb_get_setup (x_st->xcb_c)).data;

    x11_create_window (x_st, "Point Set Viewer");

    x11_setup_icccm_and_ewmh_protocols (x_st, argv[0]);

    // Crerate backbuffer pixmap
    x_st->gc = xcb_generate_id (x_st->xcb_c);
    xcb_create_gc (x_st->xcb_c, x_st->gc, x_st->window, 0, NULL);
    x_st->backbuffer = xcb_generate_id (x_st->xcb_c);
    xcb_create_pixmap (x_st->xcb_c,
                       x_st->depth,                      /* depth of the screen */
                       x_st->backbuffer,                 /* id of the pixmap */
                       x_st->window,                     /* based on this drawable */
                       x_st->screen->width_in_pixels,    /* pixel width of the screen */
                       x_st->screen->height_in_pixels);  /* pixel height of the screen */

    // Create a cairo surface on backbuffer
    //
    // NOTE: Ideally we should be using cairo_xcb_* functions, but font
    // options are ignored because the patch that added this functionality was
    // disabled, so we are forced to use cairo_xlib_* functions.
    // TODO: Check the function _cairo_xcb_screen_get_font_options () in
    // <cairo>/src/cairo-xcb-screen.c and see if the issue has been resolved, if
    // it has, then go back to xcb for consistency with the rest of the code.
    // (As of git cffa452f44e it hasn't been solved).
    Visual *Xlib_visual = Visual_from_visualid (x_st->xlib_dpy, x_st->visual->visual_id);
    cairo_surface_t *surface =
        cairo_xlib_surface_create (x_st->xlib_dpy, x_st->backbuffer, Xlib_visual,
                                   x_st->screen->width_in_pixels, x_st->screen->height_in_pixels);
    cairo_t *cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    xcb_map_window (x_st->xcb_c, x_st->window);
    xcb_flush (x_st->xcb_c);

    // ////////////////
    // Main event loop
    xcb_generic_event_t *event;
    app_graphics_t graphics;
    graphics.cr = cr;
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

    st->gui_st.set_clipboard_str = x11_set_clipboard;
    st->gui_st.get_clipboard_str = x11_get_clipboard;

    st->config = read_config (&st->memory);
    setup_db (st->config->database_location, st->config->database_source);

    while (!st->end_execution) {
        while ((event = xcb_poll_for_event (x_st->xcb_c))) {
            // NOTE: The most significant bit of event->response_type is set if
            // the event was generated from a SendEvent request, here we don't
            // care about the source of the event.
            switch (event->response_type & ~0x80) {
                case XCB_SELECTION_REQUEST:
                case XCB_SELECTION_NOTIFY:
                case XCB_SELECTION_CLEAR:
                    x11_handle_selections (x_st, &st->gui_st, event);
                    break;
                case XCB_CONFIGURE_NOTIFY:
                    {
                        graphics.width = ((xcb_configure_notify_event_t*)event)->width;
                        graphics.height = ((xcb_configure_notify_event_t*)event)->height;
                    } break;
                case XCB_MOTION_NOTIFY:
                    {
                        x_st->last_timestamp = ((xcb_motion_notify_event_t*)event)->time;
                        app_input.ptr.x = ((xcb_motion_notify_event_t*)event)->event_x;
                        app_input.ptr.y = ((xcb_motion_notify_event_t*)event)->event_y;
                    } break;
                case XCB_KEY_PRESS:
                    {
                        x_st->last_timestamp = ((xcb_key_press_event_t*)event)->time;
                        app_input.keycode = ((xcb_key_press_event_t*)event)->detail;
                        app_input.modifiers = ((xcb_key_press_event_t*)event)->state;
                    } break;
                case XCB_EXPOSE:
                    {
                        // We should tell which areas need exposing
                        force_blit = true;
                    } break;
                case XCB_BUTTON_PRESS:
                    {
                       x_st->last_timestamp = ((xcb_key_press_event_t*)event)->time;
                       char button_pressed = ((xcb_key_press_event_t*)event)->detail;
                       if (button_pressed == 4) {
                           app_input.wheel *= 1.2;
                       } else if (button_pressed == 5) {
                           app_input.wheel /= 1.2;
                       } else if (button_pressed >= 1 && button_pressed <= 3) {
                           app_input.mouse_down[button_pressed-1] = 1;
                       }
                    } break;
                case XCB_BUTTON_RELEASE:
                    {
                        x_st->last_timestamp = ((xcb_key_press_event_t*)event)->time;
                        // NOTE: This loses clicks if the button press-release
                        // sequence happens in the same batch of events, right now
                        // it does not seem to be a problem.
                        char button_pressed = ((xcb_key_press_event_t*)event)->detail;
                        if (button_pressed >= 1 && button_pressed <= 3) {
                            app_input.mouse_down[button_pressed-1] = 0;
                        }
                    } break;
                case XCB_CLIENT_MESSAGE:
                    {
                        bool handled = false;
                        xcb_client_message_event_t *client_message = ((xcb_client_message_event_t*)event);

                        // WM_DELETE_WINDOW protocol
                        if (client_message->type == xcb_atoms_cache[LOC_ATOM_WM_PROTOCOLS]) {
                            if (client_message->data.data32[0] == xcb_atoms_cache[LOC_ATOM_WM_DELETE_WINDOW]) {
                                st->end_execution = true;
                            }
                            handled = true;
                        }

                        // _NET_WM_SYNC_REQUEST protocol using the extended mode
                        if (client_message->type == xcb_atoms_cache[LOC_ATOM_WM_PROTOCOLS] &&
                            client_message->data.data32[0] == xcb_atoms_cache[LOC_ATOM__NET_WM_SYNC_REQUEST]) {

                            x_st->counter_val.lo = client_message->data.data32[2];
                            x_st->counter_val.hi = client_message->data.data32[3];
                            if (x_st->counter_val.lo % 2 != 0) {
                                increment_sync_counter (&x_st->counter_val);
                            }
                            handled = true;
                        } else if (client_message->type == xcb_atoms_cache[LOC_ATOM__NET_WM_FRAME_DRAWN]) {
                            handled = true;
                        } else if (client_message->type == xcb_atoms_cache[LOC_ATOM__NET_WM_FRAME_TIMINGS]) {
                            handled = true;
                        }

                        if (!handled) {
                            printf ("Unrecognized Client Message: %s\n",
                                    get_x11_atom_name (x_st->xcb_c,
                                                       client_message->type,
                                                       &x_st->transient_pool));
                        }
                    } break;
                case XCB_PROPERTY_NOTIFY:
                    {
                        x_st->last_timestamp = ((xcb_property_notify_event_t*)event)->time;
                    } break;
                case 0:
                    { // XCB_ERROR
                        xcb_generic_error_t *error = (xcb_generic_error_t*)event;
                        printf("Received X11 error %d\n", error->error_code);
                    } break;
                default:
                    /* Unknown event type, ignore it */
                    break;
            }
            free (event);
        }

        x11_notify_start_of_frame (x_st);

        // TODO: How bad is this? should we actually measure it?
        app_input.time_elapsed_ms = target_frame_length_ms;

        bool blit_needed = update_and_render (st, &graphics, app_input);

        cairo_status_t cr_stat = cairo_status (graphics.cr);
        if (cr_stat != CAIRO_STATUS_SUCCESS) {
            printf ("Cairo error: %s\n", cairo_status_to_string (cr_stat));
            return 0;
        }

        if (blit_needed || force_blit) {
            xcb_copy_area (x_st->xcb_c,
                           x_st->backbuffer,        /* drawable we want to paste */
                           x_st->window,            /* drawable on which we copy the previous Drawable */
                           x_st->gc,
                           0,0,0,0,
                           graphics.width,    /* pixel width of the region we want to copy */
                           graphics.height);  /* pixel height of the region we want to copy */
            force_blit = false;
        }

        x11_notify_end_of_frame (x_st);

        clock_gettime (CLOCK_MONOTONIC, &end_ticks);
        float time_elapsed = time_elapsed_in_ms (&start_ticks, &end_ticks);
        if (time_elapsed < target_frame_length_ms) {
            struct timespec sleep_ticks;
            sleep_ticks.tv_sec = 0;
            sleep_ticks.tv_nsec = (long)((target_frame_length_ms-time_elapsed)*1000000);
            nanosleep (&sleep_ticks, NULL);
        } else {
            //printf ("Frame missed! %f ms elapsed\n", time_elapsed);
        }

        clock_gettime(CLOCK_MONOTONIC, &end_ticks);
        //printf ("FPS: %f\n", 1000/time_elapsed_in_ms (&start_ticks, &end_ticks));
        start_ticks = end_ticks;

        xcb_flush (x_st->xcb_c);
        app_input.keycode = 0;
        app_input.wheel = 1;
        app_input.force_redraw = 0;
        mem_pool_end_temporary_memory (x_st->transient_pool_flush);
    }

    XCloseDisplay (x_st->xlib_dpy);
    cairo_destroy (cr);
    // NOTE: Even though we try to clean everything up Valgrind complains a
    // lot about leaking objects. Mainly inside Pango, Glib (libgobject),
    // libontconfig. Maybe all of that is global data that does not grow over
    // time but I haven't found a way to check this is the case.

    // These are not necessary but make valgrind complain less when debugging
    // memory leaks.
    gui_destroy (&st->gui_st);
    mem_pool_destroy (&st->memory);

    return 0;
}

