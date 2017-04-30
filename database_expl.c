//gcc -O2 -g -pg -Wall database_expl.c -o bin/database_expl -lcairo -lX11-xcb -lX11 -lxcb -lxcb-sync -lm -lpango-1.0 -lpangocairo-1.0 -I/usr/include/pango-1.0 -I/usr/include/cairo -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
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

#define WINDOW_HEIGHT 700
#define WINDOW_WIDTH 700

#define CANVAS_SIZE 65535 // It's the size of the coordinate axis at 1 zoom

typedef struct {
    float time_elapsed_ms;

    bool force_redraw;

    xcb_keycode_t keycode;
    uint16_t modifiers;

    float wheel;
    char mouse_down[3];

    vect2_t ptr;
} app_input_t;

void tr_canvas_to_window (transf_t tr, vect2_t *p)
{
    p->x = tr.scale*p->x+tr.dx;
    p->y = tr.scale*(CANVAS_SIZE-p->y)+tr.dy;
}

void tr_window_to_canvas (transf_t tr, vect2_t *p)
{
    p->x = (p->x - tr.dx)/tr.scale;
    p->y = (tr.scale*CANVAS_SIZE + tr.dy - p->y )/(tr.scale);
}

typedef enum {triangle, segment} entity_type;
typedef struct {
    entity_type type;
    vect3_t color;
    int line_width;

    // NOTE: Array of indexes into (app_state_t*)st->pts
    int num_points;
    int pts[3];
    char label[4];
    uint64_t id; // This is a unique id depending on the type of entity
} entity_t;

typedef enum {
    iterate_order_type,
    iterate_triangle_set,
    iterate_n,
    num_iterator_mode
} iterator_mode_t;

typedef struct {
    int i;
    char str[20];
} int_string_t;

void int_string_update (int_string_t *x, int i)
{
    x->i = i;
    snprintf (x->str, ARRAY_SIZE(x->str), "%d", i);
}

typedef enum {
    APP_POINT_SET_MODE,
    APP_GRID_MODE,
    APP_TREE_MODE,

    NUM_APP_MODES
} app_mode_t;

typedef struct {
    int n;
    vect2_t *points;
    box_t points_bb;
    double point_radius;

    double canvas_x, canvas_y;
    bool loops_computed;
    uint64_t num_loops;
    int_dyn_arr_t edges_of_all_loops;

    memory_stack_t memory;
} grid_mode_state_t;

typedef struct {
    vect2_t root_pos;

    int n;
    vect2_t *points;
    box_t points_bb;
    double point_radius;

    uint64_t num_nodes;

    int *all_perms;
    memory_stack_t memory;

    mem_pool_t pool;
} tree_mode_state_t;

typedef struct {
    int_string_t n;
    int_string_t ot_id;
    order_type_t *ot;
    int_string_t k;
    int db;

    double max_zoom;
    double zoom;
    bool zoom_changed;
    double old_zoom;
    app_graphics_t old_graphics;

    iterator_mode_t it_mode;
    int64_t user_number;

    subset_it_t *triangle_it;
    subset_it_t *triangle_set_it;

    int num_entities;
    entity_t entities[300];

    bool view_db_ot;
    vect2_t visible_pts[50];

    memory_stack_t memory;
} point_set_mode_t;

typedef struct {
    bool is_initialized;
    app_graphics_t old_graphics;

    bool end_execution;
    app_mode_t app_mode;
    point_set_mode_t *ps_mode;
    grid_mode_state_t *grid_mode;
    tree_mode_state_t *tree_mode;

    app_input_t input;
    char dragging[3];
    vect2_t ptr_delta;
    vect2_t click_coord[3];
    bool mouse_clicked[3];
    bool mouse_double_clicked[3];
    float time_since_last_click[3];
    float double_click_time;
    float min_distance_for_drag;

    css_box_t css_styles[CSS_NUM_STYLES];
    int num_layout_boxes;
    int focused_layout_box;
    layout_box_t layout_boxes[30];

    int storage_size;
    memory_stack_t memory;

    // NOTE: This will be cleared at every frame start
    memory_stack_t temporary_memory;
} app_state_t;

void triangle_entity_add (point_set_mode_t *st, int id, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = triangle;
    next_entity->color = color;
    next_entity->num_points = 3;

    next_entity->id = id;
}

void segment_entity_add (point_set_mode_t *st, int id, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = segment;
    next_entity->color = color;
    next_entity->num_points = 2;

    next_entity->id = id;
}

void draw_point (app_graphics_t *graphics, vect2_t p, char *label)
{
    cairo_t *cr = graphics->cr;
    tr_canvas_to_window (graphics->T, &p);
    cairo_arc (cr, p.x, p.y, 5, 0, 2*M_PI);
    cairo_fill (cr);

    p.x -= 10;
    p.y -= 10;
    cairo_move_to (graphics->cr, p.x, p.y);
    cairo_show_text (graphics->cr, label);
}

void draw_segment (app_graphics_t *graphics, vect2_t p1, vect2_t p2, double line_width)
{
    cairo_t *cr = graphics->cr;
    cairo_set_line_width (cr, line_width);

    tr_canvas_to_window (graphics->T, &p1);
    tr_canvas_to_window (graphics->T, &p2);

    cairo_move_to (cr, (double)p1.x, (double)p1.y);
    cairo_line_to (cr, (double)p2.x, (double)p2.y);
    cairo_stroke (cr);
}

void draw_triangle (app_graphics_t *graphics, vect2_t p1, vect2_t p2, vect2_t p3)
{
    draw_segment (graphics, p1, p2, 3);
    draw_segment (graphics, p2, p3, 3);
    draw_segment (graphics, p3, p1, 3);
}

void draw_entities (point_set_mode_t *st, app_graphics_t *graphics)
{

    int idx[3];
    int i;
    int n = st->n.i;

    vect2_t *pts = st->visible_pts;

    cairo_set_source_rgb (graphics->cr, 0, 0, 0);
    for (i=0; i<binomial(n, 2); i++) {
        subset_it_idx_for_id (i, n, idx, 2);
        vect2_t p1 = pts[idx[0]];
        vect2_t p2 = pts[idx[1]];
        draw_segment (graphics, p1, p2, 1);
    }

    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (graphics->cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case segment: {
                subset_it_idx_for_id (entity->id, n, idx, 2);
                vect2_t p1 = pts[idx[0]];
                vect2_t p2 = pts[idx[1]];
                draw_segment (graphics, p1, p2, 1);
                } break;
            case triangle: {
                subset_it_idx_for_id (entity->id, n, idx, 3);
                draw_triangle (graphics, pts[idx[0]], pts[idx[1]], pts[idx[2]]);
                } break;
            default:
                invalid_code_path;
        }
    }

    cairo_set_source_rgb (graphics->cr, 0, 0, 0);
    for (i=0; i<n; i++) {
        char str[4];
        snprintf (str, ARRAY_SIZE(str), "%i", i);
        draw_point (graphics, pts[i], str);
    }
}

// A coordinate transform can be set, by giving a point in the window, the point
// in the canvas we want mapped to it, and the zoom value.
void compute_transform (transf_t *T, vect2_t window_pt, vect2_t canvas_pt, double zoom)
{
    T->scale = zoom;
    T->dx = window_pt.x - (canvas_pt.x)*T->scale;
    T->dy = window_pt.y - (CANVAS_SIZE - canvas_pt.y)*T->scale;
}

#define WINDOW_MARGIN 40
void focus_order_type (app_graphics_t *graphics, point_set_mode_t *st)
{
    box_t box;
    get_bounding_box (st->visible_pts, st->ot->n, &box);
    double box_aspect_ratio = (double)(box.max.x-box.min.x)/(box.max.y-box.min.y);
    double window_aspect_ratio = (double)(graphics->width)/(graphics->height);
    vect2_t margins;
    if (box_aspect_ratio < window_aspect_ratio) {
        st->zoom = (double)(graphics->height - 2*WINDOW_MARGIN)/((double)(box.max.y - box.min.y));
        margins.y = WINDOW_MARGIN;
        margins.x = (graphics->width - (box.max.x-box.min.x)*st->zoom)/2;
    } else {
        st->zoom = (double)(graphics->width - 2*WINDOW_MARGIN)/((double)(box.max.x - box.min.x));
        margins.x = WINDOW_MARGIN;
        margins.y = (graphics->height - (box.max.y-box.min.y)*st->zoom)/2;
    }
    compute_transform (&graphics->T, margins, VECT2(box.min.x, box.max.y), st->zoom);
}

void set_ot (point_set_mode_t *st)
{
    int_string_update (&st->ot_id, st->ot->id);

    int n = st->n.i;
    int i;
    if (!st->view_db_ot) {
        if (st->ot->id == 0 && n>5) {
            char ot[order_type_size(n)];
            order_type_t *cvx_ot = (order_type_t*)ot;
            cvx_ot->n = st->ot->n;
            cvx_ot->id = 0;
            convex_ot (cvx_ot);
            for (i=0; i<n; i++) {
                st->visible_pts[i] = v2_from_v2i (cvx_ot->pts[i]);
            }
        } else {
            // TODO: Look for a file with a visualization of the order type
            for (i=0; i<n; i++) {
                st->visible_pts[i] = v2_from_v2i (st->ot->pts[i]);
            }
        }
    } else {
        for (i=0; i<n; i++) {
            st->visible_pts[i] = v2_from_v2i (st->ot->pts[i]);
        }
    }
}

void set_n (point_set_mode_t *st, int n, app_graphics_t *graphics)
{
    st->memory.used = 0; // Clears all storage

    int_string_update (&st->n, n);
    int t_n = thrackle_size (n);
    if (t_n == -1) {
        int_string_update (&st->k, thrackle_size_lower_bound (n));
    } else {
        int_string_update (&st->k, t_n);
    }

    st->ot = order_type_new (10, &st->memory);
    open_database (n);
    st->ot->n = n;
    db_next (st->ot);
    set_ot (st);

    st->triangle_it = subset_it_new (n, 3, &st->memory);
    subset_it_precompute (st->triangle_it);
    st->triangle_set_it = subset_it_new (st->triangle_it->size, st->k.i, &st->memory);
    focus_order_type (graphics, st);
}

// This code shows how to make a panel with 3 example buttons:
//
//    bool update_panel = false;
//    st->num_layout_boxes = 0;
//    vect2_t bg_pos = VECT2(10, 10);
//    st->layout_boxes[st->num_layout_boxes].box.min = bg_pos;
//    st->layout_boxes[st->num_layout_boxes].style = &st->css_styles[CSS_BACKGROUND];
//    st->num_layout_boxes++;
//    double x_margin = 10;
//    double y_margin = 10;
//
//    double y_pos = bg_pos.y + y_margin;
//    double width = 0, height = 0, max_width = 0;
//
//
//    char *btns[] = {"Prueba",
//                    "c",
//                    "Prueba1 larga"};
//    {
//        int i;
//        for (i=0; i<ARRAY_SIZE(btns); i++) {
//            css_box_t css_style = &st->css_styles[CSS_BUTTON];
//            css_box_compute_content_width_and_position (graphics->cr, css_style, btns[i]);
//            width = MAX (max_width, css_style->width);
//        }
//    }
//
//    if (button (btns[0], graphics->cr, bg_pos.x+x_margin, y_pos, st, &width, &height, &update_panel)) {
//        //Hacer algo.
//        printf ("Saving point set.\n");
//    }
//
//    y_pos += height+y_margin;
//    max_width = MAX (max_width, width);
//    if (button (btns[1], graphics->cr, bg_pos.x+x_margin, y_pos, st, &width, &height, &update_panel)) {
//        //Hacer algo.
//        printf ("Bla bla bla.\n");
//    }
//
//    y_pos += height+y_margin;
//    max_width = MAX (max_width, width);
//    if (button (btns[2], graphics->cr, bg_pos.x+x_margin, y_pos, st, &width, &height, &update_panel)) {
//        //Hacer algo.
//        printf ("Do other thing.\n");
//    }
//
//    y_pos += height+y_margin;
//    max_width = MAX (max_width, width);
//    st->layout_boxes[0].box.max.x = st->layout_boxes[0].box.min.x+max_width+2*x_margin;
//    st->layout_boxes[0].box.max.y = y_pos;
//
bool button (char *label, app_graphics_t *gr, double x, double y, point_set_mode_t *st,
             double *width, double *height, bool *update_panel)
{
    //cairo_t *cr = gr->cr;
    //layout_box_t *curr_box = &st->layout_boxes[st->num_layout_boxes];
    //st->num_layout_boxes++;

    //curr_box->str.s = label;
    //curr_box->style = &st->css_styles[CSS_BUTTON];
    //curr_box->status_changed = false;

    //css_box_t *css_style = &st->css_styles[CSS_BUTTON];
    //css_style->width = *width;
    //css_style->height = *height;
    //css_box_compute_content_width_and_position (cr, css_style, label);
    //*width = css_style->width;
    //*height = css_style->height;

    //curr_box->box.min.x = x;
    //curr_box->box.max.x = x + *width;
    //curr_box->box.min.y = y;
    //curr_box->box.max.y = y + *height;
    //curr_box->origin.x = css_style->content_position.x;
    //curr_box->origin.y = css_style->content_position.y;
    //bool is_ptr_over = is_point_in_box (st->click_coord[0].x, st->click_coord[0].y,
    //                         curr_box->box.min.x, curr_box->box.min.y,
    //                         *width, *height);
    //if (st->input.mouse_down[0] && is_ptr_over) {
    //    if (curr_box->status != PRESSED) {
    //        curr_box->status_changed = true;
    //        *update_panel = true;
    //    }
    //    curr_box->style = CSS_BUTTON_ACTIVE;
    //    curr_box->status = PRESSED;
    //}

    //if (st->mouse_clicked[0] && is_ptr_over) {
    //    if (curr_box->status != INACTIVE) {
    //        curr_box->status_changed = true;
    //        *update_panel = true;
    //    }
    //    curr_box->style = CSS_BUTTON;
    //    curr_box->status = INACTIVE;
    //    return true;
    //} else {
        return false;
    //}
}

typedef struct {
    double label_align;
    vect2_t label_size;
    int current_label_layout;
    layout_box_t *label_layouts;

    double entry_align;
    vect2_t entry_size;
    double row_height;
    double y_step;
    double x_step;
    double y_pos;
    double x_pos;
} labeled_entries_layout_t;

void init_labeled_layout (labeled_entries_layout_t *layout_state, app_graphics_t *graphics,
                          double x_step, double y_step, double x, double y, double width,
                          char** labels, int num_labels, app_state_t *st)
{
    double max_width = 0, max_height = 0;
    int i;

    layout_state->label_layouts = push_array (&st->temporary_memory,
                                              num_labels, layout_box_t);
    css_box_t *label_style = &st->css_styles[CSS_LABEL];
    for (i=0; i<num_labels; i++) {
        layout_box_t *curr_layout_box = &layout_state->label_layouts[i];
        curr_layout_box->str.s = labels[i];
        sized_string_compute (&curr_layout_box->str, label_style,
                              graphics->text_layout, labels[i]);
        max_width = MAX (max_width, curr_layout_box->str.width);
        max_height = MAX (max_height, curr_layout_box->str.height);
    }
    vect2_t max_string_size = VECT2(max_width, max_height);
    layout_size_from_css_content_size (label_style, &max_string_size,
                                       &layout_state->label_size);

    layout_size_from_css_content_size (&st->css_styles[CSS_TEXT_ENTRY],
                                       &max_string_size, &layout_state->entry_size);
    layout_state->row_height = MAX(layout_state->label_size.y, layout_state->entry_size.y);
    layout_state->label_size.y = layout_state->row_height;
    layout_state->entry_size.y = layout_state->row_height;

    layout_state->label_align = x;
    layout_state->entry_align = x+layout_state->label_size.x+x_step;
    layout_state->entry_size.x = width-layout_state->entry_align;
    layout_state->x_step = x_step;
    layout_state->y_step = y_step;
    layout_state->y_pos = y;
    layout_state->x_pos = x;
    layout_state->current_label_layout = 0;
}

layout_box_t* next_layout_box (app_state_t *st)
{
    layout_box_t *layout_box = &st->layout_boxes[st->num_layout_boxes];
    st->num_layout_boxes++;
    return layout_box;
}

void labeled_text_entry (char *entry_content, labeled_entries_layout_t *layout_state, app_state_t *st)
{
    vect2_t label_pos = VECT2(layout_state->label_align, layout_state->y_pos);
    vect2_t entry_pos = VECT2(layout_state->entry_align, layout_state->y_pos);

    layout_box_t *label_layout_box = next_layout_box(st);
    *label_layout_box = layout_state->label_layouts[layout_state->current_label_layout];
    layout_state->current_label_layout++;
    label_layout_box->style = &st->css_styles[CSS_LABEL];
    label_layout_box->text_align_override = CSS_TEXT_ALIGN_RIGHT;

    layout_box_t *text_entry_layout_box = next_layout_box(st);
    text_entry_layout_box->style = &st->css_styles[CSS_TEXT_ENTRY];
    text_entry_layout_box->str.s = entry_content;

    BOX_POS_SIZE (text_entry_layout_box->box, entry_pos, layout_state->entry_size);
    BOX_POS_SIZE (label_layout_box->box, label_pos, layout_state->label_size);
    layout_state->y_pos += layout_state->row_height + layout_state->y_step;
}

void title (char *str, labeled_entries_layout_t *layout_state, app_state_t *st, app_graphics_t *graphics)
{
    layout_box_t *title = next_layout_box (st);
    title->style = &st->css_styles[CSS_TITLE_LABEL];
    title->str.s = str;
    sized_string_compute (&title->str, title->style, graphics->text_layout, title->str.s);

    vect2_t content_size = VECT2 (title->str.width, title->str.height);
    vect2_t title_size;
    layout_size_from_css_content_size (title->style, &content_size, &title_size);
    BOX_POS_SIZE (title->box, VECT2(layout_state->x_pos, layout_state->y_pos), title_size);
    layout_state->y_pos += title_size.y + layout_state->y_step;
}

DRAW_CALLBACK(draw_separator)
{
    cairo_t *cr = gr->cr;
    box_t *box = &layout->box;
    cairo_set_line_width (cr, 1);
    cairo_set_source_rgba (cr, 0, 0, 0, 0.25);
    cairo_move_to (cr, box->min.x, box->min.y+0.5);
    cairo_line_to (cr, box->max.x, box->min.y+0.5);
    cairo_stroke (cr);

    cairo_set_source_rgba (cr, 1, 1, 1, 0.8);
    cairo_move_to (cr, box->min.x, box->min.y+1.5);
    cairo_line_to (cr, box->max.x, box->min.y+1.5);
    cairo_stroke (cr);
}

bool point_set_mode (app_state_t *st, app_graphics_t *graphics)
{
    point_set_mode_t *ps_mode = st->ps_mode;
    if (!ps_mode) {
        uint32_t panel_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, panel_storage);
        st->ps_mode = (point_set_mode_t*)base;
        memory_stack_init (&st->ps_mode->memory,
                           panel_storage-sizeof(point_set_mode_t),
                           base+sizeof(point_set_mode_t));

        ps_mode = st->ps_mode;
        int_string_update (&ps_mode->n, 8);
        int_string_update (&ps_mode->k, thrackle_size (ps_mode->n.i));

        ps_mode->it_mode = iterate_order_type;
        ps_mode->user_number = -1;

        ps_mode->max_zoom = 5000;
        ps_mode->zoom = 1;
        ps_mode->zoom_changed = false;
        ps_mode->old_zoom = 1;
        ps_mode->old_graphics = *graphics;
        ps_mode->view_db_ot = false;

        set_n (ps_mode, ps_mode->n.i, graphics);
    }

    app_input_t input = st->input;
    if (10 <= input.keycode && input.keycode < 20) { // KEY_0-9
        if (ps_mode->user_number ==-1) {
            ps_mode->user_number++;
        }

        int num = (input.keycode+1)%10;
        ps_mode->user_number = ps_mode->user_number*10 + num; 
        printf ("Input: %"PRIi64"\n", ps_mode->user_number);
        input.keycode = 0;
    }

    switch (input.keycode) {
        case 9: //KEY_ESC
            if (ps_mode->user_number != -1) {
                ps_mode->user_number = -1;
            } else {
                st->end_execution = 1;
            }
            break;
        case 33: //KEY_P
            print_order_type (ps_mode->ot);
            break;
        case 116://KEY_DOWN_ARROW
            if (ps_mode->zoom<ps_mode->max_zoom) {
                ps_mode->zoom += 10;
                ps_mode->zoom_changed = 1;
                input.force_redraw = 1;
            }
            break;
        case 111://KEY_UP_ARROW
            if (ps_mode->zoom>10) {
                ps_mode->zoom -= 10;
                ps_mode->zoom_changed = 1;
                input.force_redraw = 1;
            }
            break;
        case 23: //KEY_TAB
            ps_mode->it_mode = (ps_mode->it_mode+1)%num_iterator_mode;
            switch (ps_mode->it_mode) {
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
            if (ps_mode->it_mode == iterate_order_type) {
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    db_prev (ps_mode->ot);

                } else {
                    db_next (ps_mode->ot);
                }
                set_ot (ps_mode);
                focus_order_type (graphics, ps_mode);

            } else if (ps_mode->it_mode == iterate_n) {
                int n = ps_mode->n.i;
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
                set_n (ps_mode, n, graphics);
            } else if (ps_mode->it_mode == iterate_triangle_set) {
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    subset_it_prev (ps_mode->triangle_set_it);
                } else {
                    subset_it_next (ps_mode->triangle_set_it);
                }
            }
            input.force_redraw = 1;
            break;
        case 55: //KEY_V
            ps_mode->view_db_ot = !ps_mode->view_db_ot;
            set_ot (ps_mode);
            focus_order_type (graphics, ps_mode);
            input.force_redraw = 1;
            break;
        case 36: //KEY_ENTER
            if (ps_mode->it_mode == iterate_order_type) {
                db_seek (ps_mode->ot, ps_mode->user_number);
                set_ot (ps_mode);
            } else if (ps_mode->it_mode == iterate_n) {
                if (3 <= ps_mode->user_number && ps_mode->user_number <= 10) {
                    set_n (ps_mode, ps_mode->user_number, graphics);
                }
            } else if (ps_mode->it_mode == iterate_triangle_set) {
                subset_it_seek (ps_mode->triangle_set_it, ps_mode->user_number);
            }
            ps_mode->user_number = -1;
            input.force_redraw = 1;
            break;
        default:
            break;
    }

    if (input.wheel != 1) {
        ps_mode->zoom *= input.wheel;
        if (ps_mode->zoom > ps_mode->max_zoom) {
            ps_mode->zoom = ps_mode->max_zoom;
        }
        ps_mode->zoom_changed = 1;
        input.force_redraw = 1;
    }

    if (st->dragging[0]) {
        graphics->T.dx += st->ptr_delta.x;
        graphics->T.dy += st->ptr_delta.y;
        input.force_redraw = 1;
    }

    if (st->mouse_double_clicked[0]) {
        focus_order_type (graphics, ps_mode);
        input.force_redraw = 1;
    }

    // Build layout
    bool update_panel = false;
    st->num_layout_boxes = 0;
    vect2_t bg_pos = VECT2(10, 10);
    vect2_t bg_min_size = VECT2(200, 0);
    st->layout_boxes[st->num_layout_boxes].box.min = bg_pos;
    st->layout_boxes[st->num_layout_boxes].style = &st->css_styles[CSS_BACKGROUND];
    st->num_layout_boxes++;
    double x_margin = 12, x_step = 12;
    double y_margin = 12, y_step = 12;

    char *entry_labels[] = {"n:",
                            "Order Type:",
                            "k:",
                            "Triangle Set:",
                            "Edge Disj. Set:",
                            "Thrackle:"};
    labeled_entries_layout_t lay;
    init_labeled_layout (&lay, graphics, x_step, y_step,
                         bg_pos.x+x_margin, bg_pos.y+y_margin, bg_min_size.x,
                         entry_labels, ARRAY_SIZE(entry_labels), st);

    title ("Point Set", &lay, st, graphics);
    labeled_text_entry (ps_mode->n.str, &lay, st);
    labeled_text_entry (ps_mode->ot_id.str, &lay, st);
    {
        layout_box_t *sep = next_layout_box (st);
        BOX_X_Y_W_H(sep->box, lay.x_pos, lay.y_pos, bg_min_size.x-2*x_margin, 2);
        sep->draw = draw_separator;
        lay.y_pos += lay.y_step;
    }
    title ("Triangle Sets", &lay, st, graphics);
    labeled_text_entry (ps_mode->k.str, &lay, st);
    labeled_text_entry ("0", &lay, st);
    labeled_text_entry ("-", &lay, st);
    labeled_text_entry ("-", &lay, st);

    st->layout_boxes[0].box.max.x = st->layout_boxes[0].box.min.x + bg_min_size.x;
    st->layout_boxes[0].box.max.y = lay.y_pos;

    bool blit_needed = false;
    cairo_t *cr = graphics->cr;
    if (input.force_redraw) {
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_paint (cr);
        //draw_point (graphics, VECT2(100,100), "0");

        if (ps_mode->zoom_changed) {
            vect2_t userspace_ptr = input.ptr;
            tr_window_to_canvas (graphics->T, &userspace_ptr);
            compute_transform (&graphics->T, input.ptr, userspace_ptr, ps_mode->zoom);
        }

        ps_mode->old_graphics = *graphics;

        // Construct drawing on canvas.
        ps_mode->num_entities = 0;

        int i;
        get_next_color (0);
        for (i=0; i<ps_mode->k.i; i++) {
            vect3_t color;
            get_next_color (&color);
            triangle_entity_add (ps_mode, ps_mode->triangle_set_it->idx[i], color);
        }

        //printf ("dx: %f, dy: %f, s: %f, z: %f\n", graphics->T.dx, graphics->T.dy, graphics->T.scale, ps_mode->zoom);
        draw_entities (ps_mode, graphics);
        blit_needed = true;
    }

    if (blit_needed || update_panel) {
        int i;
        for (i=0; i<st->num_layout_boxes; i++) {
            if (st->layout_boxes[i].status_changed) {
                blit_needed = true;
            }
            css_box_t *style = st->layout_boxes[i].style;
            layout_box_t *layout = &st->layout_boxes[i];
            if (layout->style != NULL) {
                css_box_draw (graphics, style, layout);
            } else if (layout->draw != NULL) {
                layout->draw (graphics, layout);
            } else {
                invalid_code_path;
            }

#if 0
            box_t *rect = &st->layout_boxes[i].box;
            cairo_rectangle (cr, rect->min.x+0.5, rect->min.y+0.5, BOX_WIDTH(*rect)-1, BOX_HEIGHT(*rect)-1);
            cairo_set_source_rgba (cr, 0.3, 0.1, 0.7, 0.6);
            cairo_set_line_width (cr, 1);
            cairo_stroke (cr);
#endif
        }
    }
    return blit_needed;
}

// Calculates a ratio by which multiply box a so that it fits inside box b
double best_fit_ratio (double a_width, double a_height,
                       double b_width, double b_height)
{
    if (a_width/a_height < b_width/b_height) {
        return b_height/a_height;
    } else {
        return b_width/a_width;
    }
}

void pixel_align_as_line (vect2_t *p)
{
    p->x = floor (p->x)+0.5;
    p->y = floor (p->y)+0.5;
}

void apply_transform (transf_t *tr, vect2_t *p)
{
    p->x = tr->scale*p->x + tr->dx;
    p->y = tr->scale*p->y + tr->dy;
}

void apply_transform_distance (transf_t *tr, vect2_t *p)
{
    p->x = tr->scale*p->x;
    p->y = tr->scale*p->y;
}

void apply_inverse_transform (transf_t *tr, vect2_t *p)
{
    p->x = (p->x - tr->dx)/tr->scale;
    p->y = (p->y - tr->dy)/tr->scale;
}

void apply_inverse_transform_distance (transf_t *tr, vect2_t *p)
{
    p->x = p->x/tr->scale;
    p->y = p->y/tr->scale;
}

void compute_best_fit_box_to_box_transform (transf_t *tr, box_t *src, box_t *dest)
{
    double src_width = BOX_WIDTH(*src);
    double src_height = BOX_HEIGHT(*src);
    double dest_width = BOX_WIDTH(*dest);
    double dest_height = BOX_HEIGHT(*dest);

    tr->scale = best_fit_ratio (src_width, src_height,
                                dest_width, dest_height);
    tr->dx = dest->min.x + (dest_width-src_width*tr->scale)/2;
    tr->dy = dest->min.y + (dest_height-src_height*tr->scale)/2;
}

#define cairo_BOX(cr,box) cairo_rectangle (cr, (box).min.x, (box).min.y, BOX_WIDTH(box), BOX_HEIGHT(box));
void draw_graph (cairo_t *cr, grid_mode_state_t *grid_st, int id, box_t *dest)
{
    dest->min.x += grid_st->point_radius;
    dest->min.y += grid_st->point_radius;
    dest->max.x -= grid_st->point_radius;
    dest->max.y -= grid_st->point_radius;
    transf_t graph_to_dest;

    box_t *src = &grid_st->points_bb;
    compute_best_fit_box_to_box_transform (&graph_to_dest, src, dest);

    cairo_set_source_rgb (cr,0,0,0);
    int i;
    for (i=0; i<2*grid_st->n; i++) {
        vect2_t p = grid_st->points[i];
        apply_transform (&graph_to_dest, &p);
        cairo_arc (cr, p.x, p.y, grid_st->point_radius, 0, 2*M_PI);
        cairo_fill (cr);
    }

    for (i=0; i<2*grid_st->n; i++) {
        int e[2];
        e[0] = grid_st->edges_of_all_loops.data[id*4*grid_st->n+2*i];
        e[1] = grid_st->edges_of_all_loops.data[id*4*grid_st->n+2*i+1];
        vect2_t p1 = grid_st->points[e[0]];
        apply_transform (&graph_to_dest, &p1);
        pixel_align_as_line (&p1);

        vect2_t p2 = grid_st->points[e[1]];
        apply_transform (&graph_to_dest, &p2);
        pixel_align_as_line (&p2);

        cairo_set_line_width (cr, 1);
        cairo_move_to (cr, p1.x, p1.y);
        cairo_line_to (cr, p2.x, p2.y);
        cairo_stroke (cr);
    }
}

void bipartite_points (int n, vect2_t *points, box_t *bounding_box, double bb_ar)
{
    int64_t y_step = UINT8_MAX/(n-1);

    int64_t x_step;
    if (bb_ar == 0) {
        x_step = y_step;
    } else {
        x_step = UINT8_MAX/bb_ar;
    }
    int64_t y_pos = 0;
    int i;
    for (i=0; i<n; i++) {
        points[i].x = 0;
        points[i].y = y_pos;

        points[i+n].x = x_step;
        points[i+n].y = y_pos;
        y_pos += y_step;
    }

    if (bounding_box) {
        BOX_X_Y_W_H (*bounding_box, 0, 0, x_step, UINT8_MAX);
    }
}

bool grid_mode (app_state_t *st, app_graphics_t *gr)
{
    grid_mode_state_t *grid_mode = st->grid_mode;
    if (!st->grid_mode) {
        uint32_t panel_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, panel_storage);
        st->grid_mode = (grid_mode_state_t*)base;
        memory_stack_init (&st->grid_mode->memory,
                           panel_storage-sizeof(grid_mode_state_t),
                           base+sizeof(grid_mode_state_t));

        grid_mode = st->grid_mode;
        grid_mode->n = 3;
        grid_mode->loops_computed = false;
        grid_mode->points = push_array (&grid_mode->memory, 2*grid_mode->n, vect2_t);

        grid_mode->canvas_x = 10;
        grid_mode->canvas_y = 10;
        grid_mode->point_radius = 5;

        bipartite_points (grid_mode->n, grid_mode->points, &grid_mode->points_bb, 0);
    }

    app_input_t input = st->input;
    cairo_t *cr = gr->cr;
    if (st->dragging[0]) {
        grid_mode->canvas_x += st->ptr_delta.x;
        grid_mode->canvas_y += st->ptr_delta.y;
        input.force_redraw = true;
    }

    if (!grid_mode->loops_computed) {
        int_dyn_arr_init (&grid_mode->edges_of_all_loops, 60);
        grid_mode->num_loops = count_2_regular_subgraphs_of_k_n_n (grid_mode->n, &grid_mode->edges_of_all_loops);
        grid_mode->loops_computed = true;
    }

    if (input.force_redraw) {
        box_t dest;
        int dest_width = 40;
        int dest_height = (dest_width-2*grid_mode->point_radius)/BOX_AR(grid_mode->points_bb)+
            2*grid_mode->point_radius;

#if 0
        cairo_BOX (cr, *dest);
        cairo_set_source_rgba (cr, 0.2, 1, 0.4, 0.4);
        cairo_fill (cr);
#endif

        cairo_set_source_rgb (cr, 1,1,1);
        cairo_paint (cr);
        
        int i;
        int x_slot = 0, y_slot = 0;
        int x_step=10, y_step = 20;
        int num_x_slots = grid_mode->num_loops/binomial(grid_mode->n,2);
        for (i=0; i<grid_mode->num_loops; i++) {
            int x_pos = grid_mode->canvas_x+(dest_width+x_step)*x_slot;
            int y_pos = grid_mode->canvas_y+(dest_height+y_step)*y_slot;
            
            BOX_X_Y_W_H (dest, x_pos, y_pos, dest_width, dest_height);
            if (is_box_visible (&dest, gr)) {
                draw_graph (cr, grid_mode, i, &dest);
            }

            if (x_slot == num_x_slots-1) {
                x_slot = 0;
                y_slot++;
            } else {
                x_slot++;
            }
        }
        return true;
    }
    return false;
}


typedef enum {VTN_FULL, VTN_COMPACT, VTN_IGNORED} vtn_state_t;

struct _view_tree_node_t {
    vtn_state_t state;
    uint32_t node_id;
    uint32_t val;
    box_t box;

    uint32_t num_children;
    struct _view_tree_node_t *children[1];
};
typedef struct _view_tree_node_t view_tree_node_t;

#define push_view_node(pool, num_children) (num_children)>1? \
                       mem_pool_push_size((pool), sizeof(view_tree_node_t) + \
                       (num_children-1)*sizeof(view_tree_node_t*)) : \
                       mem_pool_push_size((pool), sizeof(view_tree_node_t))
view_tree_node_t* create_view_tree (mem_pool_t *pool, backtrack_node_t *node)
{
    view_tree_node_t *view_node = push_view_node (pool, node->num_children);
    *view_node = (view_tree_node_t){0};
    view_node->node_id = node->id;
    view_node->val = node->val;
    view_node->num_children = node->num_children;

    int ch_id;
    for (ch_id=0; ch_id<node->num_children; ch_id++) {
        view_node->children[ch_id] =
            create_view_tree (pool, node->children[ch_id]);
    }
    return view_node;
}

int view_tree_ignore (view_tree_node_t *view_node, int depth, int l)
{
    if (view_node->num_children == 0) {
        if (l < depth) {
            view_node->state = VTN_IGNORED;
        }
        return 1;
    }

    int ch_id, h = 0;
    for (ch_id=0; ch_id<view_node->num_children; ch_id++) {
        h = MAX(h, view_tree_ignore (view_node->children[ch_id], depth, l+1));
    }

    if (h+l != depth) {
        view_node->state = VTN_IGNORED;
    }

    return h+1;
}

#define view_tree_print(r) view_tree_print_helper(r,0)
void view_tree_print_helper (view_tree_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] s: %d, val: %d\n", v->node_id, v->state, v->val);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        view_tree_print_helper (v->children[ch_id], l+1);
    }
}

// The followng is an implementation of the algorithm developed in [1] to draw
// trees in linear time.
//
// [1] Buchheim, C., Jünger, M. and Leipert, S. (2006), Drawing rooted trees in
// linear time. Softw: Pract. Exper., 36: 651–665. doi:10.1002/spe.713
struct _layout_tree_node_t {
    double mod;
    double prelim;
    double change;
    double shift;
    double width;
    uint32_t node_id;
    struct _layout_tree_node_t *parent;
    struct _layout_tree_node_t *ancestor;
    struct _layout_tree_node_t *thread;
    box_t *box;
    uint32_t child_id; // position among its siblings
    uint32_t num_children;
    struct _layout_tree_node_t *children[1];
};
typedef struct _layout_tree_node_t layout_tree_node_t;

#define push_layout_node(buff, num_children) (num_children)>1? \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t) + \
                        (num_children-1)*sizeof(layout_tree_node_t*)) : \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t))

#define create_layout_tree(pool,full_width,compact_width,sibling_sep,node) \
    create_layout_tree_helper(pool, full_width, compact_width, sibling_sep, node, NULL, 0)
layout_tree_node_t* create_layout_tree_helper (mem_pool_t *pool,
                                               double full_width, double compact_width,
                                               double sibling_separation,
                                               view_tree_node_t *node,
                                               layout_tree_node_t *parent, uint32_t child_id)
{
    layout_tree_node_t *lay_node = push_layout_node (pool, node->num_children);
    *lay_node = (layout_tree_node_t){0};
    lay_node->box = &node->box;
    lay_node->ancestor = lay_node;
    lay_node->parent = parent;
    lay_node->child_id = child_id;
    lay_node->node_id = node->node_id;

    if (node->state == VTN_COMPACT) {
        lay_node->width = compact_width + sibling_separation;
    } else if (node->state == VTN_FULL) {
        lay_node->width = full_width + sibling_separation;
    }

    view_tree_node_t *not_ignored[node->num_children];
    int ch_id, num_not_ignored = 0;
    for (ch_id=0; ch_id<node->num_children; ch_id++) {
        if (node->children[ch_id]->state != VTN_IGNORED) {
            not_ignored[num_not_ignored] = node->children[ch_id];
            num_not_ignored++;
        }
    }

    if (node->state == VTN_FULL) {
        lay_node->num_children = num_not_ignored;
        for (ch_id=0; ch_id<num_not_ignored; ch_id++) {
            lay_node->children[ch_id] =
                create_layout_tree_helper (pool, full_width, compact_width,
                                           sibling_separation, not_ignored[ch_id], lay_node, ch_id);
        }
    } else {
        lay_node->num_children = 0;
    }
    return lay_node;
}

#define rightmost(v) (v->children[v->num_children-1])
#define leftmost(v) (v->children[0])
#define left_sibling(v) (v->parent->children[v->child_id-1])

layout_tree_node_t* tree_layout_next_left (layout_tree_node_t *v)
{
    if (v->num_children > 0) {
        return leftmost(v);
    } else {
        return v->thread;
    }
}

layout_tree_node_t* tree_layout_next_right (layout_tree_node_t *v)
{
    if (v->num_children > 0) {
        return rightmost(v);
    } else {
        return v->thread;
    }
}

void tree_layout_move_subtree (layout_tree_node_t *w_m, layout_tree_node_t *w_p, double shift)
{
    double subtrees = w_p->child_id - w_m->child_id;
    w_p->change -= shift/subtrees;
    w_p->shift += shift;
    w_m->change += shift/subtrees;
    w_p->prelim += shift;
    w_p->mod += shift;
}

void tree_layout_execute_shifts (layout_tree_node_t *v)
{
    double shift = 0;
    double change = 0;
    int ch_id;
    for (ch_id = v->num_children-1; ch_id >= 0; ch_id--) {
        layout_tree_node_t *w = v->children[ch_id];
        w->prelim += shift;
        w->mod += shift;
        change += w->change;
        shift += w->shift + change;
    }
}

layout_tree_node_t* tree_layout_ancestor (layout_tree_node_t *v_i_m, layout_tree_node_t *v,
                           layout_tree_node_t *default_ancestor)
{
    if (v_i_m->ancestor->parent == v->parent) {
        return v_i_m->ancestor;
    } else {
        return default_ancestor;
    }
}

void tree_layout_apportion (layout_tree_node_t *v, layout_tree_node_t **default_ancestor)
{
    layout_tree_node_t *v_i_p, *v_o_p, *v_i_m, *v_o_m;
    double s_i_p, s_o_p, s_i_m, s_o_m;
    if (v->child_id > 0) {
        v_i_p = v_o_p = v;
        v_i_m = left_sibling(v);
        v_o_m = leftmost(v_i_p->parent);
        s_i_p = v_i_p->mod;
        s_o_p = v_o_p->mod;
        s_i_m = v_i_m->mod;
        s_o_m = v_o_m->mod;
        while (tree_layout_next_right (v_i_m) != NULL && tree_layout_next_left (v_i_p) != NULL) {
            v_i_m = tree_layout_next_right (v_i_m);
            v_i_p = tree_layout_next_left (v_i_p);
            v_o_m = tree_layout_next_left (v_o_m);
            v_o_p = tree_layout_next_right (v_o_p);
            v_o_p->ancestor = v;
            double shift = (v_i_m->prelim + s_i_m) - (v_i_p->prelim + s_i_p) + v_i_m->width;
            if (shift > 0) {
                tree_layout_move_subtree (tree_layout_ancestor (v_i_m, v, *default_ancestor), v, shift);
                s_i_p += shift;
                s_o_p += shift;
            }
            s_i_m += v_i_m->mod;
            s_i_p += v_i_p->mod;
            s_o_m += v_o_m->mod;
            s_o_p += v_o_p->mod;
        }
        if (tree_layout_next_right (v_i_m) != NULL && tree_layout_next_right (v_o_p) == NULL) {
            v_o_p->thread = tree_layout_next_right (v_i_m);
            v_o_p->mod += s_i_m - s_o_p;
        }

        if (tree_layout_next_left (v_i_p) != NULL && tree_layout_next_left (v_o_m) == NULL) {
            v_o_m->thread = tree_layout_next_left (v_o_m);
            v_o_m->mod += s_i_p - s_o_m;
            *default_ancestor = v;
        }
    }
}

void tree_layout_first_walk (layout_tree_node_t *v)
{
    if (v->num_children == 0) {
        v->prelim = 0;
        if (v->child_id > 0) {
            layout_tree_node_t *w = left_sibling(v);
            v->prelim = w->prelim + w->width;
        }
    } else {
        layout_tree_node_t *default_ancestor = v->children[0];
        int ch_id;
        for (ch_id=0; ch_id<v->num_children; ch_id++) {
            layout_tree_node_t *w = v->children[ch_id];
            tree_layout_first_walk (w);
            tree_layout_apportion (w, &default_ancestor);
        }

        tree_layout_execute_shifts (v);
        int midpoint = (leftmost(v)->prelim + rightmost(v)->prelim)/2;
        if (v->child_id > 0) {
            layout_tree_node_t *w = left_sibling(v);
            v->prelim = w->prelim + w->width;
            v->mod = v->prelim - midpoint;
        } else {
            v->prelim = midpoint;
        }
    }
}

#define tree_layout_second_walk(r,size,margins) \
    tree_layout_second_walk_helper(r,size,margins,-r->prelim,0)
void tree_layout_second_walk_helper (layout_tree_node_t *v,
                                     vect2_t size, vect2_t margins,
                                     double m, double l)
{
    BOX_X_Y_W_H(*(v->box), v->prelim + m, l*(size.y+margins.y), v->width-margins.x, size.y);
    int ch_id;
    for (ch_id = 0; ch_id<v->num_children; ch_id++) {
        layout_tree_node_t *w = v->children[ch_id];
        tree_layout_second_walk_helper (w, size, margins, m+v->mod, l+1);
    }
}

#define layout_tree_preorder_print(r) layout_tree_preorder_print_helper(r,0)
void layout_tree_preorder_print_helper (layout_tree_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] x: %f, y: %f\n", v->node_id, v->box->min.x, v->box->min.y);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        layout_tree_preorder_print_helper (v->children[ch_id], l+1);
    }
}

#define backtrack_tree_preorder_print(r) backtrack_tree_preorder_print_helper(r,0)
void backtrack_tree_preorder_print_helper (backtrack_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] : %d\n", v->id, v->val);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        backtrack_tree_preorder_print_helper (v->children[ch_id], l+1);
    }
}

void draw_permutation (cairo_t *cr, uint64_t perm_id, box_t *dest, tree_mode_state_t *tree_mode)
{
    int e[2*tree_mode->n];
    edges_from_permutation (tree_mode->all_perms, perm_id, tree_mode->n, e);

    box_t l_dest = *dest;
    l_dest.min.x += tree_mode->point_radius;
    l_dest.min.y += tree_mode->point_radius;
    l_dest.max.x -= tree_mode->point_radius;
    l_dest.max.y -= tree_mode->point_radius;

    transf_t graph_to_dest;
    box_t *src = &tree_mode->points_bb;
    compute_best_fit_box_to_box_transform (&graph_to_dest, src, &l_dest);

    cairo_set_source_rgb (cr,0,0,0);
    int i;
    for (i=0; i<2*tree_mode->n; i++) {
        vect2_t p = tree_mode->points[i];
        apply_transform (&graph_to_dest, &p);
        cairo_arc (cr, p.x, p.y, tree_mode->point_radius, 0, 2*M_PI);
        cairo_fill (cr);
    }

    cairo_set_line_width (cr, 2);
    for (i=0; i<tree_mode->n; i++) {
        vect2_t p1 = tree_mode->points[e[2*i]];
        apply_transform (&graph_to_dest, &p1);
        //pixel_align_as_line (&p1);

        vect2_t p2 = tree_mode->points[e[2*i+1]];
        apply_transform (&graph_to_dest, &p2);
        //pixel_align_as_line (&p2);

        cairo_move_to (cr, p1.x, p1.y);
        cairo_line_to (cr, p2.x, p2.y);
        cairo_stroke (cr);
    }
}

void draw_view_tree_preorder (cairo_t *cr, view_tree_node_t *v, double x, double y, tree_mode_state_t *tree_mode)
{
    box_t dest_box;
    dest_box.min.x = v->box.min.x+x;
    dest_box.max.x = v->box.max.x+x;
    dest_box.min.y = v->box.min.y+y;
    dest_box.max.y = v->box.max.y+y;
    if (v->state == VTN_COMPACT || v->val == -1) {
        vect2_t p = VECT2 (dest_box.min.x + BOX_WIDTH(dest_box)/2, dest_box.min.y + BOX_HEIGHT(dest_box)/2);
        cairo_arc (cr, p.x, p.y, 7, 0, 2*M_PI);
        cairo_fill (cr);
    } else {
        draw_permutation (cr, v->val, &dest_box, tree_mode);
    }


    if (v->state == VTN_FULL) {
        int ch_id;
        for (ch_id=0; ch_id<v->num_children; ch_id++) {
            if (v->children[ch_id]->state != VTN_IGNORED) {
                draw_view_tree_preorder (cr, v->children[ch_id], x, y, tree_mode);
            }
        }
    }
}

bool tree_mode (app_state_t *st, app_graphics_t *gr)
{
    tree_mode_state_t *tree_mode = st->tree_mode;
    cairo_t *cr = gr->cr;

    static view_tree_node_t *view_root;

    int n = 5;
    if (!st->tree_mode) {
        uint32_t mode_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, mode_storage);
        st->tree_mode = (tree_mode_state_t*)base;
        memory_stack_init (&st->tree_mode->memory,
                           mode_storage-sizeof(tree_mode_state_t),
                           base+sizeof(tree_mode_state_t));
        tree_mode = st->tree_mode;
        tree_mode->root_pos = VECT2 (100, 50);
        tree_mode->n = n;
        tree_mode->point_radius = 4;
        tree_mode->points = push_array (&tree_mode->memory, 2*tree_mode->n, vect2_t);
        bipartite_points (tree_mode->n, tree_mode->points, &tree_mode->points_bb, 1.618);

        tree_mode->all_perms = mem_pool_push_array (&tree_mode->pool, factorial(n)*n, int);
        compute_all_permutations (n, tree_mode->all_perms);

        mem_pool_t bt_res_pool = {0};
        backtrack_node_t *bt_root = matching_decompositions_over_complete_bipartite_graphs (n, &bt_res_pool,
                                                                &tree_mode->num_nodes,
                                                                tree_mode->all_perms);
        view_root = create_view_tree (&tree_mode->pool, bt_root);
        view_tree_ignore (view_root, n, 0);
        //view_tree_print (view_root);
        mem_pool_destroy (&bt_res_pool);

        int dest_width = 50;
        int dest_height = (dest_width-2*tree_mode->point_radius)/BOX_AR(tree_mode->points_bb)+
            2*tree_mode->point_radius;

        vect2_t dest_size = VECT2 (dest_width, dest_height);
        vect2_t margins = VECT2 (30, 20);
        double compact_width = 14;

        int ch_id;
        for (ch_id=1; ch_id<view_root->num_children; ch_id++) {
            view_tree_node_t *w = view_root->children[ch_id];
            if (w->state != VTN_IGNORED) {
                w->state = VTN_COMPACT;
            }
        }

        mem_pool_t layout_tree = {0};
        layout_tree_node_t *root = create_layout_tree (&layout_tree, dest_size.x,
                                                       compact_width, margins.x, view_root);
        root->width = compact_width + margins.x;

        tree_layout_first_walk (root);
        tree_layout_second_walk (root, dest_size, margins);
        //layout_tree_preorder_print (root);
        //printf ("\n");
        mem_pool_destroy (&layout_tree);
    }

    if (st->dragging[0]) {
        tree_mode->root_pos.x += st->ptr_delta.x;
        tree_mode->root_pos.y += st->ptr_delta.y;
        st->input.force_redraw = true;
    }

    if (st->input.force_redraw) {
        cairo_set_source_rgb (cr, 1,1,1);
        cairo_paint (cr);
        cairo_set_source_rgb (cr,0,0,0);
        draw_view_tree_preorder (cr, view_root, tree_mode->root_pos.x, tree_mode->root_pos.y, tree_mode);

        return true;
    }
    return false;
}

bool update_and_render (app_state_t *st, app_graphics_t *graphics, app_input_t input)
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

    xcb_connection_t *connection = XGetXCBConnection (dpy); //just in case we need XCB
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

    int storage_size =  megabyte(60);
    uint8_t* memory = calloc (storage_size, 1);
    app_state_t *st = (app_state_t*)memory;
    st->storage_size = storage_size;
    memory_stack_init (&st->memory, st->storage_size-sizeof(app_state_t), memory+sizeof(app_state_t));

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

