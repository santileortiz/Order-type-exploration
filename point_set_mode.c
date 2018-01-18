/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#define CANVAS_SIZE 65535 // It's the size of the coordinate axis at 1 zoom

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

void draw_point (cairo_t *cr, vect2_t p, char *label, double radius, transf_t *T)
{
    if (T != NULL) {
        apply_transform (T, &p);
    }
    cairo_arc (cr, p.x, p.y, radius, 0, 2*M_PI);
    cairo_fill (cr);

    p.x -= 2*radius;
    p.y -= 2*radius;
    cairo_move_to (cr, p.x, p.y);
    cairo_show_text (cr, label);
}

void draw_polygon (cairo_t *cr, vect2_t *polygon, int len, transf_t *T)
{
    vect2_t pts[len];
    memcpy (pts, polygon, sizeof(vect2_t)*len);
    int i;
    for (i=0; i<len; i++) {
        apply_transform (T, &pts[i]);
    }

    cairo_new_path (cr);
    cairo_move_to (cr, pts[0].x, pts[0].y);
    cairo_line_to (cr, pts[1].x, pts[1].y);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_stroke (cr);

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to (cr, pts[1].x, pts[1].y);
    for (i=2; i<len; i++) {
        cairo_line_to (cr, pts[i].x, pts[i].y);
    }
    cairo_line_to (cr, pts[0].x, pts[0].y);
    cairo_stroke (cr);
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
}

void draw_triangle (cairo_t *cr, vect2_t p1, vect2_t p2, vect2_t p3, transf_t *T)
{
    draw_segment (cr, p1, p2, 3, T);
    draw_segment (cr, p2, p3, 3, T);
    draw_segment (cr, p3, p1, 3, T);
    cairo_stroke (cr);
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
    cairo_stroke (cr);

    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case segment: {
                subset_it_idx_for_id (entity->id, n, idx, 2);
                vect2_t p1 = pts[idx[0]];
                vect2_t p2 = pts[idx[1]];
                draw_segment (cr, p1, p2, 1, T);
                cairo_stroke (cr);
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
        draw_point (cr, pts[i], str, st->point_radius, T);
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

// The following is an implementation of an algorithm that positions the points
// in a point set automatically into a more friendly configuration than the one
// found in the database. A more in depth discussion about it's design can be
// found in the file auto_positioning_points.txt.

void angular_force (vect2_t *points, order_type_t *ot, int v, int s_m, int p, int s_p, double h, vect2_t *res)
{
    vect2_t vs_m = vect2_subs (points[s_m], points[v]);
    vect2_t vp = vect2_subs (points[p], points[v]);
    vect2_t vs_p = vect2_subs (points[s_p], points[v]);

    double ang_a = vect2_clockwise_angle_between (vs_m, vp);
    double ang_b = vect2_clockwise_angle_between (vp, vs_p);
    if (!in_cone (vs_m, VECT2(0,0), vs_p, vp)) {
        double ang_c = vect2_clockwise_angle_between (vs_m, vs_p);
        if (vect2_clockwise_angle_between (vs_p, vp) < (2*M_PI-ang_c)/2) {
            ang_b = -vect2_angle_between (vs_p, vp);
        } else {
            ang_a = -vect2_angle_between (vs_m, vp);
        }
    }

    vect2_t force_p = VECT2 (vp.y, -vp.x); // Clockwise perpendicular vector to vp
    vect2_normalize (&force_p);
    vect2_mult_to (&force_p, (ang_b-ang_a)/(ang_a+ang_b)*h);
    vect2_add_to (&res[p], force_p);

    //if (p==0) {
    //    printf ("v: %d, (%d, %d, %d) = ", v, s_m, p, s_p);
    //    vect2_print (&force_p);
    //}
}

// Computes the force between two points proportional to the difference in their
// distance and length. When adding the resulting vector to a and it's inverse
// to b it effectively moves points closer to being at distance length. The
// value h is a positive value that determines the strength of the force.
vect2_t spring_force_pts (vect2_t a, vect2_t b, double length, double h)
{
    vect2_t res;
    vect2_t force = vect2_subs (b, a);
    double d = vect2_norm (force);
    if (d > 0) {
        vect2_normalize (&force);
        res = vect2_mult (force, h*(d-length));
    } else {
        res = VECT2(0,0);
    }
    return res;
}

vect2_t spring_force (vect2_t *points, int a, int b, double length, double h)
{
    vect2_t p = points[a];
    vect2_t p_next = points[b];
    return spring_force_pts (p, p_next, length, h);
}

void arrange_points_start (struct arrange_points_state_t *alg_st, order_type_t *ot, vect2_t *points)
{
    int len = ot->n;
    *alg_st = (struct arrange_points_state_t){0};
    alg_st->ot = ot;
    alg_st->sort = mem_pool_push_size(&alg_st->pool, sizeof(int)*len*(len-1));
    int *sort_ptr = alg_st->sort;
    int i;
    for (i=0; i<len; i++) {
        sort_all_points_p (ot, i, 0, sort_ptr);
        sort_ptr += len-1;
    }

    alg_st->cvx_hull = mem_pool_push_size(&alg_st->pool, sizeof(int)*len);
    convex_hull (ot, alg_st->cvx_hull, &alg_st->cvx_hull_len);

    vect2_t cvx_hull_v2[len];
    vect2_idx_to_array (points, alg_st->cvx_hull, cvx_hull_v2, alg_st->cvx_hull_len);
    alg_st->centroid = polygon_centroid (cvx_hull_v2, alg_st->cvx_hull_len);

    alg_st->tgt_hull = mem_pool_push_size(&alg_st->pool, sizeof(vect2_t)*alg_st->cvx_hull_len);
    convex_point_set_radius (alg_st->cvx_hull_len, 0,
                             points[alg_st->cvx_hull[0]], alg_st->centroid,
                             alg_st->tgt_hull);
    alg_st->tgt_radius = vect2_norm (vect2_subs(points[alg_st->cvx_hull[0]], alg_st->centroid));
    alg_st->steps = 0;
}

// Returns true if the resulting point set is good enough to show to the user.
bool arrange_points_end (struct arrange_points_state_t *alg_st, vect2_t *points, int len)
{
    bool res = true;
    int i;
    for (i=0; i < len; i++) {
        vect2_round (&points[i]);
    }

    box_t box;
    get_bounding_box (points, len, &box);

    for (i=0; i < len; i++) {
        vect2_subs_to (&points[i], VECT2(box.min.x, box.min.y));
    }

    ot_triples_t *trip_ot = ot_triples_new (alg_st->ot, &alg_st->pool);
    ot_triples_t *trip_pts = ps_triples_new (points, len, &alg_st->pool);
    if (!ot_triples_are_equal (trip_ot, trip_pts)) {
        res = false;
    }

    mem_pool_destroy(&alg_st->pool);
    return res;
}

double arrange_points_step (struct arrange_points_state_t *alg_st, vect2_t *points, int len)
{
    order_type_t *ot = alg_st->ot;
    alg_st->steps++;

    vect2_t res[len];
    int i;
    for (i=0; i<len; i++) {
        res[i] = (vect2_t){0};
    }

    // Compute the sum of all angular forces on a point p
    vect2_t tmp_forces[len];
    for (i=0; i<len; i++) {
        tmp_forces[i] = (vect2_t){0};
    }
    int v;
    for (v=0; v<len; v++) {
        int *pts_arnd_v = &alg_st->sort[v*(len-1)];

        double magnitude = alg_st->tgt_radius/(30*len);
        angular_force (points, ot, v, pts_arnd_v[len-2], pts_arnd_v[0],
                       pts_arnd_v[1], magnitude, tmp_forces);
        int j;
        for (j=0; j<len-3; j++) {
            int s_m = pts_arnd_v[j];
            int p = pts_arnd_v[j+1];
            int s_p = pts_arnd_v[j+2];
            angular_force (points, ot, v, s_m, p, s_p, magnitude, tmp_forces);
        }

        angular_force (points, ot, v, pts_arnd_v[j], pts_arnd_v[j+1],
                       pts_arnd_v[0], magnitude, tmp_forces);
    }

    for (i=0; i<len; i++) {
        vect2_mult_to (&tmp_forces[i], 1);
        vect2_add_to (&res[i], tmp_forces[i]);
    }

    // Circular wall
    for (i=0; i<alg_st->cvx_hull_len; i++) {
        vect2_t r = vect2_subs (points[alg_st->cvx_hull[i]], alg_st->centroid);
        double dist = vect2_norm(r) - alg_st->tgt_radius;
        if (dist > 0) {
            vect2_normalize (&r);
            double norm = vect2_dot (res[alg_st->cvx_hull[i]], r);
            vect2_mult_to (&r, -(norm + 0.01*dist));
            vect2_add_to (&res[alg_st->cvx_hull[i]], r);
        }
    }

    //for (i=0; i<len; i++) {
    //    vect2_print (&res[i]);
    //}

    // Move the original points by the computed force and compute the change
    // indicator.
    double change = 0;
    vect2_t old_p_0 = points[0];
    vect2_add_to (&points[0], res[0]);
    //vect2_print (&res[0]);
    for (i=1; i<len; i++) {
        double old_dist = vect2_distance (&old_p_0, &points[i]);
        vect2_add_to (&points[i], res[i]);
        //vect2_print (&res[i]);
        double new_dist = vect2_distance (&points[0], &points[i]);
        change = MAX(change, fabs(old_dist-new_dist));
    }

    return change*75/alg_st->tgt_radius;
}

void move_hitbox (struct point_set_mode_t *ps_mode)
{
    struct gui_state_t *gui_st = global_gui_st;
    int i = ps_mode->active_hitbox;
    layout_box_t *hitbox = &ps_mode->pts_hitboxes[i];
    transf_t *T = &ps_mode->points_to_canvas;

    vect2_t delta_canvas = gui_st->ptr_delta;
    apply_inverse_transform_distance (T, &delta_canvas);
    vect2_add_to (&ps_mode->visible_pts[i], delta_canvas);

    vect2_add_to (&hitbox->box.min, gui_st->ptr_delta);
    vect2_add_to (&hitbox->box.max, gui_st->ptr_delta);
}

void move_hitbox_int (struct point_set_mode_t *ps_mode)
{
    struct gui_state_t *gui_st = global_gui_st;
    int i = ps_mode->active_hitbox;
    layout_box_t *hitbox = &ps_mode->pts_hitboxes[i];
    transf_t *T = &ps_mode->points_to_canvas;

    vect2_t ptr = gui_st->input.ptr;
    apply_inverse_transform (T, &ptr);
    vect2_round (&ptr);

    ps_mode->visible_pts[i] = ptr;
    vect2_add_to (&hitbox->box.min, gui_st->ptr_delta);
    vect2_add_to (&hitbox->box.max, gui_st->ptr_delta);
}

struct arrange_work_t {
    struct point_set_mode_t *ps_mode;
    volatile uint64_t *curr_n;
    uint64_t n; // n at the time the thread was started.
    order_type_t *ot;
};

void* arrange_points_work (void *arg)
{
    struct arrange_work_t *wk = (struct arrange_work_t *)arg;
    struct point_set_mode_t *ps_mode = wk->ps_mode;

    struct arrange_points_state_t alg_st;
    double change;
    arrange_points_start (&alg_st, wk->ot, ps_mode->arranged_pts);

    struct timespec start_time, curr_time;
    clock_gettime (CLOCK_MONOTONIC, &start_time);
    float time_elapsed;
    do {
        change = arrange_points_step (&alg_st, ps_mode->arranged_pts, wk->n);
        clock_gettime (CLOCK_MONOTONIC, &curr_time);
        time_elapsed = time_elapsed_in_ms (&start_time, &curr_time);
    } while (change > 0.01 && time_elapsed < 100 && wk->n == *(wk->curr_n));

    //if (wk->n == *(wk->curr_n)) {
    //    printf ("Point autopositioning was cancelled\n");
    //} else if (time_elapsed >= 100) {
    //    printf ("Point autopositioning timed out.\n");
    //}

    ps_mode->ot_arrangeable = arrange_points_end (&alg_st, ps_mode->arranged_pts, wk->n);
    mem_pool_destroy (&global_gui_st->thread_pool);
    pthread_exit(0);
}

void set_ot (struct point_set_mode_t *ps_mode)
{
    int_string_update (&ps_mode->ot_id, ps_mode->ot->id);

    int n = ps_mode->n.i;
    int i;
    for (i=0; i<n; i++) {
        vect2_t pt = v2_from_v2i (ps_mode->ot->pts[i]);
        ps_mode->visible_pts[i] = pt;
        ps_mode->arranged_pts[i] = pt;
    }

    pthread_join (global_gui_st->thread, NULL);
    struct arrange_work_t *wk =
        mem_pool_push_size (&global_gui_st->thread_pool, sizeof(struct arrange_work_t));
    wk->ps_mode = ps_mode;
    wk->n = ps_mode->n.i;
    wk->curr_n = &ps_mode->n.i;
    wk->ot = order_type_copy (&global_gui_st->thread_pool, ps_mode->ot);

    ps_mode->ot_arrangeable = false;
    ps_mode->redraw_panel = true;
    pthread_create (&global_gui_st->thread, NULL, arrange_points_work, wk);
}

void set_k (struct point_set_mode_t *st, int k)
{
    int_string_update (&st->k, k);
    st->num_triangle_subsets = binomial(binomial(st->n.i,3),k);
}

void set_n (struct point_set_mode_t *st, int n, app_graphics_t *graphics)
{
    mem_pool_destroy (&st->memory);

    int_string_update (&st->n, n);

    int initial_k = thrackle_size (n);
    if (initial_k == -1) {
        initial_k = thrackle_size_lower_bound (n);
    }

    st->ot = order_type_new (10, &st->memory);
    open_database (n);
    st->ot->n = n;
    db_next (st->ot);
    set_ot (st);

    st->triangle_it = subset_it_new (n, 3, &st->memory);
    subset_it_precompute (st->triangle_it);
    set_k (st, initial_k);
    focus_order_type (graphics, st);
}

#define next_layout_box(st) next_layout_box_css(st,CSS_NONE)
layout_box_t* next_layout_box_css (struct app_state_t *st, css_style_t style_id)
{
    assert (st->num_layout_boxes+1 < ARRAY_SIZE (st->layout_boxes));
    layout_box_t *layout_box = &st->layout_boxes[st->num_layout_boxes];

    // NOTE: Even though we zero initialize app_state, if we reuse the layout
    // boxes then we should initialize them to 0.
    *layout_box = (layout_box_t){0};
    st->num_layout_boxes++;

    if (style_id != CSS_NONE) {
        init_layout_box_style (&st->gui_st, layout_box, style_id);
    }

    return layout_box;
}

bool update_button_state (struct app_state_t *st, layout_box_t *lay_box, bool *update_panel)
{
    bool retval = false;
    if (!(lay_box->active_selectors & CSS_SEL_DISABLED) &&
        st->gui_st.mouse_clicked[0] &&
        is_vect2_in_box (global_gui_st->click_coord[0], lay_box->box) &&
        (lay_box->active_selectors & CSS_SEL_HOVER)) {
        retval = true;
    }
    return retval;
}

layout_box_t* button (char *label, bool *target, app_graphics_t *gr,
                      double x, double y, double *width, double *height,
                      struct app_state_t *st)
{
    layout_box_t *curr_box = next_layout_box_css (st, CSS_BUTTON);

    curr_box->str.s = label;

    vect2_t layout_size;
    if (width == NULL || height == NULL) {
        sized_string_compute (&curr_box->str, curr_box->style, gr->text_layout, label);
        vect2_t str_size = VECT2(curr_box->str.width, curr_box->str.height);

        layout_size_from_css_content_size (curr_box->style, &str_size, &layout_size);
    }
    if (width == NULL) {
        layout_size.x = layout_size.x;
        *width = layout_size.x;
    } else {
        layout_size.x = *width;
    }
    if (height == NULL) {
        layout_size.y = layout_size.y;
        *height = layout_size.y;
    } else {
        layout_size.y = *height;
    }
    curr_box->box.min.x = x;
    curr_box->box.max.x = x + *width;
    curr_box->box.min.y = y;
    curr_box->box.max.y = y + *height;

    add_behavior (&st->gui_st, curr_box, BEHAVIOR_BUTTON, target);
    focus_chain_add (&st->gui_st, curr_box);
    *target = update_button_state (st, curr_box, NULL);
    return curr_box;
}

layout_box_t* label (char *str, double x, double y, struct app_state_t *st, app_graphics_t *graphics)
{
    layout_box_t *downloading_lbl = next_layout_box_css (st, CSS_LABEL);

    downloading_lbl->str.s = str;
    struct css_box_t *label_style = &st->gui_st.css_styles[CSS_LABEL];
    sized_string_compute (&downloading_lbl->str, label_style,
                          graphics->text_layout, downloading_lbl->str.s);
    BOX_CENTER_X_Y_W_H (downloading_lbl->box, x, y,
                        downloading_lbl->str.width, downloading_lbl->str.height);
    return downloading_lbl;
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

    layout_state->label_layouts =
        mem_pool_push_size_full (&st->temporary_memory,(num_labels)*sizeof(layout_box_t),POOL_ZERO_INIT);

    struct css_box_t *label_style = &st->gui_st.css_styles[CSS_LABEL];
    for (i=0; i<num_labels; i++) {
        layout_box_t *curr_layout_box = &layout_state->label_layouts[i];
        init_layout_box_style (&st->gui_st, curr_layout_box, CSS_LABEL);

        curr_layout_box->str.s = labels[i];
        sized_string_compute (&curr_layout_box->str, label_style,
                              graphics->text_layout, labels[i]);
        max_width = MAX (max_width, curr_layout_box->str.width);
        max_height = MAX (max_height, curr_layout_box->str.height);
    }
    vect2_t max_string_size = VECT2(max_width, max_height);
    layout_size_from_css_content_size (label_style, &max_string_size,
                                       &layout_state->label_size);

    layout_size_from_css_content_size (&st->gui_st.css_styles[CSS_TEXT_ENTRY],
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

layout_box_t* labeled_text_entry (uint64_string_t *target,
                                  labeled_entries_layout_t *layout_state,
                                  struct app_state_t *st)
{
    vect2_t label_pos = VECT2(layout_state->label_align, layout_state->y_pos);
    vect2_t entry_pos = VECT2(layout_state->entry_align, layout_state->y_pos);

    layout_box_t *label_layout_box = next_layout_box (st);
    *label_layout_box = layout_state->label_layouts[layout_state->current_label_layout];
    layout_state->current_label_layout++;

    label_layout_box->text_align_override = CSS_TEXT_ALIGN_RIGHT;

    layout_box_t *text_entry_layout_box = next_layout_box_css (st, CSS_TEXT_ENTRY);
    focus_chain_add (&st->gui_st, text_entry_layout_box);
    text_entry_layout_box->str.s = str_data (&target->str);
    add_behavior (&st->gui_st, text_entry_layout_box, BEHAVIOR_TEXT_ENTRY, target);

    BOX_POS_SIZE (text_entry_layout_box->box, entry_pos, layout_state->entry_size);
    BOX_POS_SIZE (label_layout_box->box, label_pos, layout_state->label_size);
    layout_state->y_pos += layout_state->row_height + layout_state->y_step;
    return text_entry_layout_box;
}

layout_box_t* labeled_button (char *button_text, bool *target,
                               labeled_entries_layout_t *layout_state,
                               struct app_state_t *st, app_graphics_t *gr)
{
    vect2_t label_pos = VECT2(layout_state->label_align, layout_state->y_pos);
    vect2_t button_pos = VECT2(layout_state->entry_align, layout_state->y_pos);

    layout_box_t *label_layout_box = next_layout_box_css (st, CSS_LABEL);
    *label_layout_box = layout_state->label_layouts[layout_state->current_label_layout];
    layout_state->current_label_layout++;

    label_layout_box->text_align_override = CSS_TEXT_ALIGN_RIGHT;

    BOX_POS_SIZE (label_layout_box->box, label_pos, layout_state->label_size);
    layout_state->y_pos += layout_state->row_height + layout_state->y_step;
    layout_box_t *btn =
        button (button_text, target, gr, button_pos.x, button_pos.y,
                &layout_state->entry_size.x, &layout_state->entry_size.y, st);
    init_layout_box_style (&st->gui_st, btn, CSS_BUTTON);
    return btn;
}

void title (char *str, labeled_entries_layout_t *layout_state, struct app_state_t *st, app_graphics_t *graphics)
{
    layout_box_t *title = next_layout_box_css (st, CSS_TITLE_LABEL);

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

void set_focused_box_value (struct point_set_mode_t *ps_mode, app_graphics_t *graphics, uint64_t val)
{
    layout_box_t *focused_box = global_gui_st->focus->dest;
    if (ps_mode->focus_list[foc_ot] == focused_box) {
        if (val >= __g_db_data.num_order_types) {
            val = __g_db_data.num_order_types-1;
        }
        db_seek (ps_mode->ot, val);
        set_ot (ps_mode);
        focus_order_type (graphics, ps_mode);

    } else if (ps_mode->focus_list[foc_n] == focused_box) {
        if (val < 3) {
            val = 3;
        } else if (val > 10) {
            val = 10;
        }
        set_n (ps_mode, val, graphics);

    } else if (ps_mode->focus_list[foc_k] == focused_box) {
        uint64_t max_k = ps_mode->triangle_it->size;
        if (val > max_k) {
            val = max_k;
        }
        set_k (ps_mode, val);

    } else if (ps_mode->focus_list[foc_ts] == focused_box) {
        if (val >= ps_mode->num_triangle_subsets) {
            val = ps_mode->num_triangle_subsets-1;
        }
        int_string_update (&ps_mode->ts_id, val);
    }

    ps_mode->redraw_canvas = true;
}

bool is_negative (char *str)
{
    while (*str != '\0') {
        if (*str == '-') {
            return true;
        }
        str++;
    }
    return false;
}

bool point_set_mode (struct app_state_t *st, app_graphics_t *graphics)
{
    struct point_set_mode_t *ps_mode = st->ps_mode;
    struct gui_state_t *gui_st = &st->gui_st;
    if (!ps_mode) {
        st->ps_mode =
            mem_pool_push_size_full (&st->memory, sizeof(struct point_set_mode_t), POOL_ZERO_INIT);

        ps_mode = st->ps_mode;
        int_string_update (&ps_mode->n, 8);
        int_string_update (&ps_mode->k, thrackle_size (ps_mode->n.i));

        ps_mode->max_zoom = 5000;
        ps_mode->zoom = 1;
        ps_mode->zoom_changed = false;
        ps_mode->old_zoom = 1;
        ps_mode->view_db_ot = true;
        ps_mode->point_radius = 5;

        ps_mode->points_to_canvas.dx = WINDOW_MARGIN;
        ps_mode->points_to_canvas.dy = WINDOW_MARGIN;
        ps_mode->points_to_canvas.scale_x = (double)(graphics->height-2*WINDOW_MARGIN)/CANVAS_SIZE;
        ps_mode->points_to_canvas.scale_y = ps_mode->points_to_canvas.scale_x;

        ps_mode->rebuild_panel = true;
        ps_mode->redraw_canvas = true;

        set_n (ps_mode, ps_mode->n.i, graphics);
        int_string_update (&ps_mode->ts_id, 0);
    }

    app_input_t input = st->gui_st.input;
    switch (input.keycode) {
        case 33: //KEY_P
            print_order_type (ps_mode->ot);
            if (ps_mode->ot_arrangeable) {
                printf ("Arranged points:\n");
                int i;
                for (i=0; i<ps_mode->n.i; i++) {
                    printf ("(%.1f, %.1f)\n", ps_mode->visible_pts[i].x, ps_mode->visible_pts[i].y);
                }
            }
            printf ("\n");
            break;
        case 116://KEY_DOWN_ARROW
            break;
        case 111://KEY_UP_ARROW
            break;
        case 23: //KEY_TAB
            if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers) {
                focus_prev (gui_st);
            } else {
                focus_next (gui_st);
            }
            ps_mode->redraw_panel = true;
            break;
        case 57: //KEY_N
        case 113://KEY_LEFT_ARROW
        case 114://KEY_RIGHT_ARROW
            {
            layout_box_t *focused_box = gui_st->focus->dest;
            if (ps_mode->focus_list[foc_ot] == focused_box) {
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                    || input.keycode == 113) {
                    db_prev (ps_mode->ot);

                } else {
                    db_next (ps_mode->ot);
                }
                set_ot (ps_mode);
                focus_order_type (graphics, ps_mode);

            } else if (ps_mode->focus_list[foc_n] == focused_box) {
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
            } else if (ps_mode->focus_list[foc_k] == focused_box) {
                uint64_t max_k = ps_mode->triangle_it->size;
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    if (ps_mode->k.i != 0) {
                        set_k (ps_mode, ps_mode->k.i-1);
                    }
                } else {
                    if (ps_mode->k.i < max_k) {
                        set_k (ps_mode, ps_mode->k.i+1);
                    }
                }
            } else if (ps_mode->focus_list[foc_ts] == focused_box) {
                uint64_t id = ps_mode->ts_id.i;
                uint64_t last_id = ps_mode->num_triangle_subsets-1;
                if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers
                        || input.keycode == 113) {
                    if (id == 0) {
                        id = last_id;
                    } else {
                        id--;
                    }
                } else {
                    if (id == last_id) {
                        id = 0;
                    } else {
                        id++;
                    }
                }
                int_string_update (&ps_mode->ts_id, id);
            }
            ps_mode->redraw_panel = true;
            ps_mode->redraw_canvas = true;
            } break;
        case 54: //KEY_C
            if (XCB_KEY_BUT_MASK_CONTROL & input.modifiers) {
                if (gui_st->selection.dest != NULL) {
                    gui_st->platform.set_clipboard_str (gui_st->selection.start,
                                                        gui_st->selection.len);
                }
            }
            break;
        case 55: //KEY_V
            if (XCB_KEY_BUT_MASK_CONTROL & input.modifiers) {
                gui_st->platform.get_clipboard_str ();
            } else {
                ps_mode->view_db_ot = !ps_mode->view_db_ot;
                set_ot (ps_mode);
                focus_order_type (graphics, ps_mode);
                ps_mode->redraw_canvas = true;
            }
            break;
        default:
            break;
    }

    if (input.wheel != 1) {
        ps_mode->zoom *= input.wheel;
        if (ps_mode->zoom > ps_mode->max_zoom) {
            ps_mode->zoom = ps_mode->max_zoom;
        }
        ps_mode->zoom_changed = true;
        ps_mode->redraw_canvas = true;
    }

    // Build layout
    static layout_box_t *panel;
    if (ps_mode->rebuild_panel) {
        st->num_layout_boxes = 0;
        vect2_t bg_pos = VECT2(10, 10);
        vect2_t bg_min_size = VECT2(200, 0);

        panel = next_layout_box_css (st, CSS_BACKGROUND);
        panel->box.min = bg_pos;
        double x_margin = 12, x_step = 12;
        double y_margin = 12, y_step = 12;

        char *entry_labels[] = {"n:",
                                "Order Type:",
                                "Layout:",
                                "k:",
                                "ID:"};
        labeled_entries_layout_t lay;
        init_labeled_layout (&lay, graphics, x_step, y_step,
                             bg_pos.x+x_margin, bg_pos.y+y_margin, bg_min_size.x,
                             entry_labels, ARRAY_SIZE(entry_labels), st);

        title ("Point Set", &lay, st, graphics);
        ps_mode->focus_list[foc_n] = labeled_text_entry (&ps_mode->n, &lay, st);
        ps_mode->focus_list[foc_ot] = labeled_text_entry (&ps_mode->ot_id, &lay, st);
        ps_mode->arrange_pts_btn = labeled_button ("Arrange",
                                                        &ps_mode->arrange_pts,
                                                        &lay, st, graphics);
        focus_chain_remove (gui_st, ps_mode->arrange_pts_btn);
        {
            layout_box_t *sep = next_layout_box (st);
            BOX_X_Y_W_H(sep->box, lay.x_pos, lay.y_pos, bg_min_size.x-2*x_margin, 2);
            sep->draw = draw_separator;
            lay.y_pos += lay.y_step;
        }

        title ("Triangle Set", &lay, st, graphics);
        ps_mode->focus_list[foc_k] = labeled_text_entry (&ps_mode->k, &lay, st);
        ps_mode->focus_list[foc_ts] = labeled_text_entry (&ps_mode->ts_id, &lay, st);

        panel->box.max.x = st->layout_boxes[0].box.min.x + bg_min_size.x;
        panel->box.max.y = lay.y_pos;

        ps_mode->redraw_panel = true;
    }

    if (ps_mode->arrange_pts) {
        int i;
        for (i=0; i<ps_mode->n.i; i++) {
            ps_mode->visible_pts[i] = ps_mode->arranged_pts[i];
        }
        focus_order_type (graphics, ps_mode);
        ps_mode->redraw_canvas = true;
    }

    if (ps_mode->ot_arrangeable) {
        selector_unset (ps_mode->arrange_pts_btn, CSS_SEL_DISABLED);
    } else {
        selector_set (ps_mode->arrange_pts_btn, CSS_SEL_DISABLED);
    }

    // I'm still not sure about handling selection here in the future. On one
    // hand it seems like we would want a selection to be a unique thing in
    // gui_st and handle it only once and do the right thing for different kinds
    // of selections (like here). On the other it seems to be tightly coupled to
    // the handling of BEHAVIOR_TEXT_ENTRY. Should this better be handled inside
    // the text entry FSM?, or have a BEHAVIOR_SELECTION with a changing
    // target?. This should get sorted out when implementing a complete unicode
    // text entry that has a cursor and more advanced selection.
    if (gui_st->clipboard_ready && gui_st->selection.dest != NULL) {
        char *val;
        if (is_negative (gui_st->clipboard_str)) {
            val = "0";
        } else {
            val = gui_st->clipboard_str;
        }

        layout_box_t *selected_box = gui_st->selection.dest;
        if (selected_box->behavior->type == BEHAVIOR_TEXT_ENTRY) {
            str_cpy (&ps_mode->bak_number.str, &selected_box->behavior->target.u64_str->str);
            int_string_update_s (selected_box->behavior->target.u64_str, val);
        }
    }

    update_layout_boxes (&st->gui_st, st->layout_boxes, st->num_layout_boxes,
                         &ps_mode->redraw_panel);

    update_layout_boxes (&st->gui_st, ps_mode->pts_hitboxes,
                         ps_mode->n.i,
                         &ps_mode->redraw_canvas);

    struct behavior_t *beh = st->gui_st.behaviors;
    int i = 0;
    while (beh != NULL) {
        switch (beh->type) {
            case BEHAVIOR_BUTTON:
                *beh->target.b = update_button_state (st, beh->box, &ps_mode->redraw_panel);
                break;
            case BEHAVIOR_TEXT_ENTRY:
                {
                    // NOTE: This is not a normal text entry behavior, here we
                    // have no cursor, and we always select all the content.
                    struct layout_box_t *lay_box = beh->box;

                    switch (beh->state) {
                        case 0:
                            {
                                if (SEL_RISING_EDGE(lay_box,CSS_SEL_FOCUS) ||
                                   (gui_st->mouse_clicked[0] && lay_box->active_selectors & CSS_SEL_HOVER))
                                {
                                    select_all_str (gui_st, lay_box, lay_box->str.s);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 1;
                                }
                            } break;
                        case 1:
                            {
                                if ((gui_st->mouse_double_clicked[0] &&
                                     lay_box->active_selectors & CSS_SEL_HOVER))
                                {
                                    // If the user double clicks, expecting the
                                    // usual "select all" behavior, then we do
                                    // that too, instead of unselecting the
                                    // selection made by a single click.
                                    //
                                    // Do Nothing.
                                } else if (10 <= input.keycode && input.keycode < 20) { // KEY_0-9
                                    unselect (gui_st);

                                    str_cpy (&ps_mode->bak_number.str, &beh->target.u64_str->str);
                                    int digit = (input.keycode+1)%10;
                                    int_string_update (beh->target.u64_str, digit);

                                    ps_mode->redraw_panel = true;
                                    beh->state = 2;

                                } else if (gui_st->clipboard_ready) {
                                    unselect (gui_st);
                                    gui_st->clipboard_ready = false;
                                    ps_mode->redraw_panel = true;
                                    beh->state = 2;

                                } else if (36 == input.keycode || // KEY_ENTER
                                            9 == input.keycode || // KEY_ESC
                                            (gui_st->mouse_clicked[0] && lay_box->active_selectors & CSS_SEL_HOVER)) {
                                    unselect (gui_st);

                                    beh->state = 0;
                                    ps_mode->redraw_panel = true;
                                } else if (!(lay_box->active_selectors & CSS_SEL_FOCUS)) {
                                    beh->state = 0;
                                    ps_mode->redraw_panel = true;
                                }
                            } break;
                        case 2:
                            {
                                if (gui_st->mouse_clicked[0] && lay_box->active_selectors & CSS_SEL_HOVER) {
                                    select_all_str (gui_st, lay_box, lay_box->str.s);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 1;
                                } else if (10 <= input.keycode && input.keycode < 20) { // KEY_0-9
                                    int digit = (input.keycode+1)%10;
                                    int_string_append_digit (beh->target.u64_str, digit);
                                    ps_mode->redraw_panel = true;
                                } else if (36 == input.keycode || // KEY_ENTER
                                           !(lay_box->active_selectors & CSS_SEL_FOCUS)) {
                                    lay_box->content_changed = true;
                                    lay_box->str.s = str_data (&beh->target.u64_str->str);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 0;
                                } else if (9 == input.keycode) { // KEY_ESC
                                    str_cpy (&beh->target.u64_str->str, &ps_mode->bak_number.str);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 0;
                                }
                            } break;
                        default:
                            invalid_code_path;
                    }
                } break;
            default:
                invalid_code_path;
        }
        beh = beh->next;
        i++;
    }

    for (i=1; i<num_focus_options; i++) {
        layout_box_t *curr_layout_box = ps_mode->focus_list[i];
        if (curr_layout_box->content_changed) {
            ps_mode->redraw_canvas = true;
            uint64_t val;
            switch (i) {
                case foc_n:
                    val = ps_mode->n.i;
                    if (val < 3) {
                        val = 3;
                    } else if (val > 10) {
                        val = 10;
                    }
                    set_n (ps_mode, val, graphics);
                    break;
                case foc_ot:
                    val = ps_mode->ot_id.i;
                    if (val >= __g_db_data.num_order_types) {
                        val = __g_db_data.num_order_types-1;
                    } 
                    db_seek (ps_mode->ot, val);
                    set_ot (ps_mode);
                    focus_order_type (graphics, ps_mode);
                    break;
                case foc_k:
                    val = ps_mode->k.i;
                    uint64_t max_k = ps_mode->triangle_it->size;
                    if (val > max_k) {
                        val = max_k;
                    }
                    set_k (ps_mode, val);
                    break;
                case foc_ts:
                    val = ps_mode->ts_id.i;
                    if (val >= ps_mode->num_triangle_subsets) {
                        val = ps_mode->num_triangle_subsets-1;
                    }
                    int_string_update (&ps_mode->ts_id, val);
                    break;
            }
            break;
        }
    }

    // Update canvas state
    bool ptr_clicked_in_canvas = false;
    switch (ps_mode->canvas_state) {
        // TODO: Maybe convert this into a DRAG behavior once all the hitbox
        // logic can be replaced by a more advanced visibility computation of
        // the layout boxes.
        //
        // Or, we can also create a DRAGGING selector flag even though CSS
        // doesn't have such thing.
        case 0:
            {
                if (!is_vect2_in_box (gui_st->click_coord[0], panel->box) && gui_st->input.mouse_down[0]) {
                    bool hit = false;
                    int i;
                    for (i=0; i<ps_mode->n.i; i++) {
                        layout_box_t *hitbox = &ps_mode->pts_hitboxes[i];

                        if (is_vect2_in_box (gui_st->click_coord[0], hitbox->box)) {
                            ps_mode->active_hitbox = i;
                            move_hitbox_int (ps_mode);

                            hit = true;
                            ps_mode->redraw_canvas = true;
                            ps_mode->canvas_state = 1;
                            break;
                        }
                    }

                    if (!hit) {
                        transform_translate (&ps_mode->points_to_canvas, &gui_st->ptr_delta);
                        ptr_clicked_in_canvas = true;
                        ps_mode->redraw_canvas = true;
                        ps_mode->canvas_state = 2;
                    }
                }
            } break;
        case 1:
            {
                if (gui_st->input.mouse_down[0]) {
                    move_hitbox_int (ps_mode);
                    ps_mode->redraw_canvas = true;
                } else {
                    ps_mode->canvas_state = 0;
                }
            } break;
        case 2:
            if (gui_st->input.mouse_down[0]) {
                transform_translate (&ps_mode->points_to_canvas, &gui_st->ptr_delta);
                ps_mode->redraw_canvas = true;
                ps_mode->canvas_state = 2;
            } else {
                ps_mode->canvas_state = 0;
            }
            break;
        default:
            invalid_code_path;

    }

    if (gui_st->mouse_double_clicked[0] && ptr_clicked_in_canvas) {
        focus_order_type (graphics, ps_mode);
        ps_mode->redraw_canvas = true;
    }

    bool blit_needed = false;
    cairo_t *cr = graphics->cr;
    if (input.force_redraw || ps_mode->redraw_canvas) {
        cairo_clear (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_paint (cr);

        if (ps_mode->zoom_changed) {
            vect2_t window_ptr_pos = input.ptr;
            apply_inverse_transform (&ps_mode->points_to_canvas, &window_ptr_pos);
            compute_transform (&ps_mode->points_to_canvas, input.ptr, window_ptr_pos, ps_mode->zoom);
        }

        // Points may have changed position suddenly, update the hitboxes.
        int i;
        for (i=0; i<ARRAY_SIZE(ps_mode->pts_hitboxes); i++) {
            layout_box_t *curr_hitbox = &ps_mode->pts_hitboxes[i];

            vect2_t point = ps_mode->visible_pts[i];
            apply_transform (&ps_mode->points_to_canvas, &point);

            BOX_CENTER_X_Y_W_H(curr_hitbox->box,
                               point.x, point.y,
                               3*ps_mode->point_radius, 3*ps_mode->point_radius);
        }

        // If ts_id is outside of bounds, clamp it.
        if (ps_mode->ts_id.i >= ps_mode->num_triangle_subsets) {
            int_string_update (&ps_mode->ts_id, ps_mode->num_triangle_subsets-1);
        }

        // Construct drawing on canvas.
        ps_mode->num_entities = 0;

        get_next_color (0);
        int triangles [ps_mode->k.i];
        subset_it_idx_for_id (ps_mode->ts_id.i, ps_mode->triangle_it->size,
                              triangles, ARRAY_SIZE(triangles));
        for (i=0; i<ps_mode->k.i; i++) {
            vect3_t color;
            get_next_color (&color);
            triangle_entity_add (ps_mode, triangles[i], color);
        }

        draw_entities (ps_mode, graphics);

        ps_mode->redraw_panel = true;
        blit_needed = true;
    }

    if (ps_mode->redraw_panel) {
        int i;
        for (i=0; i<st->num_layout_boxes; i++) {
            struct css_box_t *style = st->layout_boxes[i].style;
            layout_box_t *layout = &st->layout_boxes[i];
            if (layout->style != NULL) {
                css_box_draw (graphics, style, layout);
            } else if (layout->draw != NULL) {
                layout->draw (graphics, layout);
            } else {
                invalid_code_path;
            }

        }
        blit_needed = true;
    }

    layout_boxes_end_frame (st->layout_boxes, st->num_layout_boxes);
    layout_boxes_end_frame (ps_mode->pts_hitboxes, ps_mode->n.i);

    ps_mode->rebuild_panel = false;
    ps_mode->redraw_panel = false;
    ps_mode->redraw_canvas = false;

    return blit_needed;
}
