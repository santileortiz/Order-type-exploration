#include"app_api.h"
#include "point_set_mode.h"

#define CANVAS_SIZE 65535 // It's the size of the coordinate axis at 1 zoom
void int_string_update (int_string_t *x, int i)
{
    x->i = i;
    snprintf (x->str, ARRAY_SIZE(x->str), "%d", i);
}

void triangle_entity_add (struct point_set_mode_t *st, int id, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = triangle;
    next_entity->color = color;
    next_entity->num_points = 3;

    next_entity->id = id;
}

void segment_entity_add (struct point_set_mode_t *st, int id, vect3_t color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = segment;
    next_entity->color = color;
    next_entity->num_points = 2;

    next_entity->id = id;
}

void draw_point (cairo_t *cr, vect2_t p, char *label, transf_t *T)
{
    if (T != NULL) {
        apply_transform (T, &p);
    }
    cairo_arc (cr, p.x, p.y, 5, 0, 2*M_PI);
    cairo_fill (cr);

    p.x -= 10;
    p.y -= 10;
    cairo_move_to (cr, p.x, p.y);
    cairo_show_text (cr, label);
}

void draw_segment (cairo_t *cr, vect2_t p1, vect2_t p2, double line_width, transf_t *T)
{
    cairo_set_line_width (cr, line_width);

    if (T != NULL) {
        apply_transform (T, &p1);
        apply_transform (T, &p2);
    }

    cairo_move_to (cr, (double)p1.x, (double)p1.y);
    cairo_line_to (cr, (double)p2.x, (double)p2.y);
    cairo_stroke (cr);
}

void draw_triangle (cairo_t *cr, vect2_t p1, vect2_t p2, vect2_t p3, transf_t *T)
{
    draw_segment (cr, p1, p2, 3, T);
    draw_segment (cr, p2, p3, 3, T);
    draw_segment (cr, p3, p1, 3, T);
}

void draw_entities (struct point_set_mode_t *st, app_graphics_t *graphics)
{
    int idx[3];
    int i;
    int n = st->n.i;

    vect2_t *pts = st->visible_pts;
    cairo_t *cr = graphics->cr;
    transf_t *T = &st->points_to_canvas;

    cairo_set_source_rgb (cr, 0, 0, 0);
    for (i=0; i<binomial(n, 2); i++) {
        subset_it_idx_for_id (i, n, idx, 2);
        vect2_t p1 = pts[idx[0]];
        vect2_t p2 = pts[idx[1]];
        draw_segment (cr, p1, p2, 1, T);
    }

    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case segment: {
                subset_it_idx_for_id (entity->id, n, idx, 2);
                vect2_t p1 = pts[idx[0]];
                vect2_t p2 = pts[idx[1]];
                draw_segment (cr, p1, p2, 1, T);
                } break;
            case triangle: {
                subset_it_idx_for_id (entity->id, n, idx, 3);
                draw_triangle (cr, pts[idx[0]], pts[idx[1]], pts[idx[2]], T);
                } break;
            default:
                invalid_code_path;
        }
    }

    cairo_set_source_rgb (cr, 0, 0, 0);
    for (i=0; i<n; i++) {
        char str[4];
        snprintf (str, ARRAY_SIZE(str), "%i", i);
        draw_point (cr, pts[i], str, T);
    }
}

// A coordinate transform can be set, by giving a point in the window, the point
// in the canvas we want mapped to it, and the zoom value.
void compute_transform (transf_t *T, vect2_t window_pt, vect2_t canvas_pt, double zoom)
{
    T->scale_x = zoom;
    T->scale_y = -zoom;
    T->dx = window_pt.x - canvas_pt.x*T->scale_x;
    T->dy = window_pt.y - canvas_pt.y*T->scale_y;
}

#define WINDOW_MARGIN 40
void focus_order_type (app_graphics_t *graphics, struct point_set_mode_t *st)
{
    box_t box;
    get_bounding_box (st->visible_pts, st->ot->n, &box);
    st->zoom = best_fit_ratio (BOX_WIDTH(box), BOX_HEIGHT(box),
                               graphics->width - 2*WINDOW_MARGIN,
                               graphics->height - 2*WINDOW_MARGIN);

    double x_center = ((graphics->width - 2*WINDOW_MARGIN) - BOX_WIDTH(box)*st->zoom)/2;
    double y_center = ((graphics->height - 2*WINDOW_MARGIN) - BOX_HEIGHT(box)*st->zoom)/2;
    compute_transform (&st->points_to_canvas,
                       VECT2(WINDOW_MARGIN+x_center, WINDOW_MARGIN+y_center),
                       VECT2(box.min.x, box.max.y), st->zoom);
}

void set_ot (struct point_set_mode_t *st)
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

void set_n (struct point_set_mode_t *st, int n, app_graphics_t *graphics)
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
bool button (char *label, app_graphics_t *gr, double x, double y, struct point_set_mode_t *st,
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
                          char** labels, int num_labels, struct app_state_t *st)
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

layout_box_t* next_layout_box (struct app_state_t *st)
{
    layout_box_t *layout_box = &st->layout_boxes[st->num_layout_boxes];
    st->num_layout_boxes++;
    return layout_box;
}

void labeled_text_entry (char *entry_content, labeled_entries_layout_t *layout_state, struct app_state_t *st)
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

void title (char *str, labeled_entries_layout_t *layout_state, struct app_state_t *st, app_graphics_t *graphics)
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

bool point_set_mode (struct app_state_t *st, app_graphics_t *graphics)
{
    struct point_set_mode_t *ps_mode = st->ps_mode;
    if (!ps_mode) {
        uint32_t panel_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, panel_storage);
        st->ps_mode = (struct point_set_mode_t*)base;
        memory_stack_init (&st->ps_mode->memory,
                           panel_storage-sizeof(struct point_set_mode_t),
                           base+sizeof(struct point_set_mode_t));

        ps_mode = st->ps_mode;
        int_string_update (&ps_mode->n, 8);
        int_string_update (&ps_mode->k, thrackle_size (ps_mode->n.i));

        ps_mode->it_mode = iterate_order_type;
        ps_mode->user_number = -1;

        ps_mode->max_zoom = 5000;
        ps_mode->zoom = 1;
        ps_mode->zoom_changed = false;
        ps_mode->old_zoom = 1;
        ps_mode->view_db_ot = false;

        ps_mode->points_to_canvas.dx = WINDOW_MARGIN;
        ps_mode->points_to_canvas.dy = WINDOW_MARGIN;
        ps_mode->points_to_canvas.scale_x = (double)(graphics->height-2*WINDOW_MARGIN)/CANVAS_SIZE;
        ps_mode->points_to_canvas.scale_y = ps_mode->points_to_canvas.scale_x;

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
        ps_mode->points_to_canvas.dx += st->ptr_delta.x;
        ps_mode->points_to_canvas.dy += st->ptr_delta.y;
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
            vect2_t window_ptr_pos = input.ptr;
            apply_inverse_transform (&ps_mode->points_to_canvas, &window_ptr_pos);
            compute_transform (&ps_mode->points_to_canvas, input.ptr, window_ptr_pos, ps_mode->zoom);
        }

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
