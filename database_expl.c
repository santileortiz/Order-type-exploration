//gcc -g database_expl.c -o database_expl -I/usr/include/cairo -lcairo -lX11
// Dependencies:
// sudo apt-get install libxcb1-dev libcairo2-dev
#include <X11/Xlib.h>
#include <X11/Xatom.h>
//#include <xcb/xcb.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cairo-xlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <math.h>
#include <errno.h>

// Used for profiling
#include <time.h>

#define WINDOW_HEIGHT 300
#define WINDOW_WIDTH 300

#define N 5

#if N<9
typedef uint8_t coord_t;
#define MAX_COORD_VAL 255
#else
typedef uint16_t coord_t;
#define MAX_COORD_VAL 65535
#endif

///////////////////////////
// TIMERS (Profiling tools)
// Functions used for profiling and timming in general:
//  - A process clock that measures time used by this process.
//  - A wall clock that measures real world time.

struct timespec proc_clock_info;
struct timespec wall_clock_info;
void  validate_clock (struct timespec *clock_info) {
    if (clock_info->tv_sec != 0 || clock_info->tv_nsec != 1) {
        printf ("Error: We expect a 1ns resolution timer. %li s %li ns resolution instead.\n",
                clock_info->tv_sec, clock_info->tv_nsec);
    }
}

void setup_clocks ()
{
    if (-1 == clock_getres (CLOCK_PROCESS_CPUTIME_ID, &proc_clock_info)) {
        printf ("Error: could not get profiling clock resolution\n");
    }
    validate_clock (&proc_clock_info);

    if (-1 == clock_getres (CLOCK_MONOTONIC, &wall_clock_info)) {
        printf ("Error: could not get wall clock resolution\n");
    }
    validate_clock (&wall_clock_info);
    return;
}

void print_time_elapsed (struct timespec *time_start, struct timespec *time_end, char *str)
{
    float time_in_ms = (time_end->tv_sec*1000-time_start->tv_sec*1000+
                       (float)(time_end->tv_nsec-time_start->tv_nsec)/1000000);
    if (time_in_ms < 1000) {
        printf ("[%f ms]: %s\n", time_in_ms, str);
    } else {
        printf ("[%f s]: %s\n", time_in_ms/1000, str);
    }
}

// Process clock
// Usage:
//   BEGIN_TIME_BLOCK;
//   <some code to measure>
//   END_TIME_BLOCK("Name of timed block");

struct timespec proc_ticks_start;
struct timespec proc_ticks_end;
#define BEGIN_TIME_BLOCK {clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &proc_ticks_start);}
#define END_TIME_BLOCK(str) {\
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &proc_ticks_end);\
    print_time_elapsed(&proc_ticks_start, &proc_ticks_end, str);\
    }

// Wall timer
// Usage:
//   BEGIN_WALL_CLOCK; //call once
//   <arbitrary code>
//   PROBE_WALL_CLOCK("Name of probe 1");
//   <arbitrary code>
//   PROBE_WALL_CLOCK("Name of probe 2");

struct timespec wall_ticks_start;
struct timespec wall_ticks_end;
#define BEGIN_WALL_CLOCK {clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &wall_ticks_start);}
#define PROBE_WALL_CLOCK(str) {\
    clock_gettime(CLOCK_MONOTONIC, &wall_ticks_end);\
    print_time_elapsed(&wall_ticks_start, &wall_ticks_end, str);\
    wall_ticks_start = wall_ticks_end;\
    }

// There are several way of doing this, using HSV with fixed SV and random Hue
// incremented every PHI_INV seems good enough. If it ever stops being good,
// research about perception based formats like CIELAB (LAB), and color
// difference functions like CIE76, CIE94, CIEDE2000.
#define PHI_INV 0.618033988749895
void get_next_color (uint8_t *r, uint8_t *g, uint8_t *b)
{
    static double h = 0;
    double s = 0.99;
    double v = 0.99;
    h += PHI_INV;

}

typedef struct {
    coord_t x;
    coord_t y;
} point_t;

typedef struct {
    point_t points [N];
} order_type_t;

void print_order_type (order_type_t *ot)
{
    int i = 0;
    while (i < N) {
        printf ("(%" PRIu8 ",%" PRIu8 ")\n", ot->points[i].x, ot->points[i].y);
        i++;
    }
    printf ("\n");
}

typedef struct {
    double x;
    double y;
} vect2_t;

#define VECT2(x,y) ((vect2_t){x,y})

typedef struct {
    xcb_keycode_t keycode;
    uint16_t modifiers;

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
    vp->scale_factor = (vp->min_edge-WINDOW_MARGIN)*zoom/MAX_COORD_VAL;
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

// This function sets the view so that _pt_ (in order type coordinates) is in
// the lower left corner of the view area. _zoom_ is given as a percentage.
//
//   +------------------+    
//   +------------------+    
//   |               W  |
//   |    +-------+     |     W: Window
//   |    |       |     |     A: View Area
//   |    |   A   |     |     o: position of _pt_
//   |    |       |     |
//   |    o-------+     |\
//   |                  || -> WINDOW_MARGIN
//   +------------------+/
//
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

void draw_point (cairo_t *cr, double x, double y)
{
    double r = 3, n=0;
    cairo_device_to_user_distance (cr, &r, &n);
    cairo_arc (cr, x, y, r, 0, 2*M_PI);
    cairo_fill (cr);
}

int end_execution = 0;
char redraw = 0;
void update_and_render (app_graphics_t *graphics, app_input_t input)
{
    static int app_is_initialized = 0;
    static order_type_t ot;
    static int db;
    static cairo_matrix_t transf;
    static app_graphics_t old_graphics;
    static double max_zoom = 200;
    static double zoom = 100;
    static char zoom_changed = 0;
    static double old_zoom = 100;
    static vect2_t origin = {0, 0};
    static view_port_t vp;


    if (!app_is_initialized) {
        // Opening Order Type database
        char filename[13];
        sprintf (filename, "otypes%02d.b%02lu", N, sizeof(coord_t)*8);
        db = open (filename, O_RDONLY);
        read (db, &ot, sizeof (order_type_t));
        app_is_initialized = 1;

        old_graphics = *graphics;
        update_transform (*graphics, 100, VECT2(0,0), &vp);
        redraw = 1;
    }

    switch (input.keycode) {
        case 9: //KEY_ESC
        case 24: //KEY_Q
            end_execution = 1;
            return;
        case 33: //KEY_P
            print_order_type (&ot);
            break;
        case 116://KEY_DOWN_ARROW
            if (zoom<max_zoom) {
                zoom += 10;
                zoom_changed = 1;
                redraw = 1;
            }
            break;
        case 111://KEY_UP_ARROW
            if (zoom>10) {
                zoom -= 10;
                zoom_changed = 1;
                redraw = 1;
            }
            break;
        case 57: //KEY_N
        case 113://KEY_LEFT_ARROW
        case 114://KEY_RIGHT_ARROW
            if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                    || input.keycode == 113) {
                if (-1 == lseek (db, -2*sizeof(order_type_t), SEEK_CUR)) {
                    if (errno == EINVAL) {
                        lseek (db, -sizeof(order_type_t), SEEK_END);
                    }
                }
            }

            if(!read (db, &ot, sizeof (order_type_t))) {
                lseek (db, 0, SEEK_SET);
                read (db, &ot, sizeof (order_type_t));
            } 
            redraw = 1;
            break;
        default:
            printf ("%" PRIu8 "\n", input.keycode);
            //printf ("%" PRIu16 "\n", input.modifiers);
            break;
    }

    if (redraw) {
        cairo_t *cr = graphics->cr;
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_paint (cr);

        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_new_path (cr);

        if (old_graphics.width != graphics->width || old_graphics.height != graphics->height) {
            update_transform (*graphics, zoom, vp.origin, &vp);
            
        } else if (zoom_changed) {
            view_port_t new_vp;
            vect2_t canvas_ptr = input.ptr;
            cairo_device_to_user (graphics->cr, &canvas_ptr.x, &canvas_ptr.y);

            update_transform_fixed_pt (*graphics, zoom, canvas_ptr, &vp, &new_vp);
            vp = new_vp;
            zoom_changed = 0;
        }

        old_graphics = *graphics;

        int i=0, j=0;
        while (i < N) {
            draw_point (cr, (double)ot.points[i].x, (double)ot.points[i].y);
            i++;
        }

        double r = 1, n=0;
        cairo_device_to_user_distance (cr, &r, &n);
        cairo_set_line_width (cr, r);

        for (i=0; i<N; i++) {
            for (j=i+1; j<N; j++) {
                double x1, y1, x2, y2;
                cairo_move_to (cr, (double)ot.points[i].x, (double)ot.points[i].y);
                cairo_line_to (cr, (double)ot.points[j].x, (double)ot.points[j].y);
                cairo_stroke (cr);
            }
        }

        cairo_surface_flush(cairo_get_target (cr));
        redraw = 0;
    }
}

int main (void)
{
    // Setup clocks
    setup_clocks ();

    //////////////////
    // X11 setup

    Display *dpy = XOpenDisplay (NULL);
    xcb_connection_t *c;

    if (!dpy) {
        printf ("Could not open display\n");
        return -1;
    }

    c = XGetXCBConnection (dpy); //just in case we need XCB
    if (!c) {
        printf ("Could not get XCB connection from Xlib Display\n");
        return -1;
    }

    /* Get the default screen */
    // TODO: What happens if there is more than 1 screen?.
    Screen *screen = XDefaultScreenOfDisplay (dpy);

    // Create a window
    uint32_t mask = CWBackPixel | CWEventMask;

    XSetWindowAttributes values;
    values.background_pixel = WhitePixel(dpy, XDefaultScreen (dpy));
    values.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask |
                        ButtonReleaseMask | PointerMotionMask;

    Window window = XCreateWindow (dpy,    /* connection          */
              RootWindowOfScreen(screen),  /* parent window       */
              0, 0,                        /* x, y                */
              WINDOW_WIDTH, WINDOW_HEIGHT, /* width, height       */
              10,                          /* border_width        */
              CopyFromParent,              /* depth               */
              InputOutput,                 /* class               */
              CopyFromParent,              /* visual              */
              mask, &values);              /* attributes          */

    // Set window title
    char *title = "Order Type";
    XChangeProperty (dpy,
            window,
            XA_WM_NAME,
            XA_STRING,
            8,
            PropModeReplace,
            title,
            strlen(title));

    XMapWindow (dpy, window);
    XFlush (dpy);

    // Getting visual of root window (required to get a cairo surface)
    Visual *default_visual = DefaultVisualOfScreen (screen);

    // Create a cairo surface on window
    cairo_surface_t *surface = cairo_xlib_surface_create (dpy, window, default_visual, WINDOW_WIDTH, WINDOW_HEIGHT);
    cairo_t *cr = cairo_create (surface);
    cairo_surface_destroy(surface);

    // ////////////////
    // Main event loop
    xcb_generic_event_t *event;
    app_graphics_t graphics;
    graphics.cr = cr;
    graphics.width = WINDOW_WIDTH;
    graphics.height = WINDOW_HEIGHT;

    char str[500];
    uint32_t timer_start = 0;
    BEGIN_WALL_CLOCK;

    app_input_t app_input = {0};
    while (!end_execution) {
        event = xcb_wait_for_event (c);
        switch (event->response_type) {
            case XCB_CONFIGURE_NOTIFY:
                graphics.width = ((xcb_configure_notify_event_t*)event)->width;
                graphics.height = ((xcb_configure_notify_event_t*)event)->height;
                cairo_xlib_surface_set_size (cairo_get_target (graphics.cr),
                        graphics.width,
                        graphics.height);
                break;
            case XCB_MOTION_NOTIFY:
                app_input.ptr.x = ((xcb_motion_notify_event_t*)event)->event_x;
                app_input.ptr.y = ((xcb_motion_notify_event_t*)event)->event_y;
                break;
            case XCB_KEY_PRESS:
                app_input.keycode = ((xcb_key_press_event_t*)event)->detail;
                app_input.modifiers = ((xcb_key_press_event_t*)event)->state;
                break;
            case XCB_EXPOSE:
                // We should tell which areas need exposing
                redraw = 1;
                break;
            case XCB_BUTTON_PRESS:
                break;
            case XCB_BUTTON_RELEASE:
                break;
            default: 
                /* Unknown event type, ignore it */
                continue;
        }
        update_and_render (&graphics, app_input);
        app_input.keycode = 0;
        free (event);
        xcb_flush (c);
    }

    return 0;
}
