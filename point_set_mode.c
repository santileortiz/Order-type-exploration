/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#define CANVAS_SIZE 65535 // It's the size of the coordinate axis at 1 zoom

void triangle_entity_add (struct point_set_mode_t *st, int id, dvec3 color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = triangle;
    next_entity->color = color;
    next_entity->num_points = 3;

    next_entity->id = id;
}

void segment_entity_add (struct point_set_mode_t *st, int id, dvec3 color)
{
    assert (st->num_entities < ARRAY_SIZE(st->entities));
    entity_t *next_entity = &st->entities[st->num_entities];
    st->num_entities++;

    next_entity->type = segment;
    next_entity->color = color;
    next_entity->num_points = 2;

    next_entity->id = id;
}

void draw_point (cairo_t *cr, dvec2 p, char *label, double radius, transf_t *T)
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

void draw_polygon (cairo_t *cr, dvec2 *polygon, int len, transf_t *T)
{
    dvec2 pts[len];
    memcpy (pts, polygon, sizeof(dvec2)*len);
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

void draw_segment (cairo_t *cr, dvec2 p1, dvec2 p2, double line_width, transf_t *T)
{
    cairo_set_line_width (cr, line_width);

    if (T != NULL) {
        apply_transform (T, &p1);
        apply_transform (T, &p2);
    }

    cairo_move_to (cr, (double)p1.x, (double)p1.y);
    cairo_line_to (cr, (double)p2.x, (double)p2.y);
}

void draw_triangle (cairo_t *cr, dvec2 p1, dvec2 p2, dvec2 p3, transf_t *T)
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

    dvec2 *pts = st->visible_pts;
    cairo_t *cr = graphics->cr;
    transf_t *T = &st->points_to_canvas;

    cairo_set_source_rgb (cr, 0, 0, 0);
    for (i=0; i<binomial(n, 2); i++) {
        subset_it_idx_for_id (i, n, idx, 2);
        dvec2 p1 = pts[idx[0]];
        dvec2 p2 = pts[idx[1]];
        draw_segment (cr, p1, p2, 1, T);
    }
    cairo_stroke (cr);

    for (i=0; i<st->num_entities; i++) {
        entity_t *entity = &st->entities[i];
        cairo_set_source_rgb (cr, entity->color.r, entity->color.g, entity->color.b);
        switch (entity->type) {
            case segment: {
                subset_it_idx_for_id (entity->id, n, idx, 2);
                dvec2 p1 = pts[idx[0]];
                dvec2 p2 = pts[idx[1]];
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
        char str[11];
        snprintf (str, ARRAY_SIZE(str), "%i", i);
        draw_point (cr, pts[i], str, st->point_radius, T);
    }
}

// A coordinate transform can be set, by giving a point in the window, the point
// in the canvas we want mapped to it, and the zoom value.
void compute_transform (transf_t *T, dvec2 window_pt, dvec2 canvas_pt, double zoom)
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
                       DVEC2(WINDOW_MARGIN+x_center, WINDOW_MARGIN+y_center),
                       DVEC2(box.min.x, box.max.y), st->zoom);
    st->redraw_canvas = true;
}

// The following is an implementation of an algorithm that positions the points
// in a point set automatically into a more friendly configuration than the one
// found in the database. A more in depth discussion about it's design can be
// found in the file auto_positioning_points.txt.

void angular_force (dvec2 *points, order_type_t *ot, int v, int s_m, int p, int s_p, double h, dvec2 *res)
{
    dvec2 vs_m = dvec2_subs (points[s_m], points[v]);
    dvec2 vp = dvec2_subs (points[p], points[v]);
    dvec2 vs_p = dvec2_subs (points[s_p], points[v]);

    double ang_a = dvec2_clockwise_angle_between (vs_m, vp);
    double ang_b = dvec2_clockwise_angle_between (vp, vs_p);
    if (!in_cone (vs_m, DVEC2(0,0), vs_p, vp)) {
        double ang_c = dvec2_clockwise_angle_between (vs_m, vs_p);
        if (dvec2_clockwise_angle_between (vs_p, vp) < (2*M_PI-ang_c)/2) {
            ang_b = -dvec2_angle_between (vs_p, vp);
        } else {
            ang_a = -dvec2_angle_between (vs_m, vp);
        }
    }

    dvec2 force_p = DVEC2 (vp.y, -vp.x); // Clockwise perpendicular vector to vp
    dvec2_normalize (&force_p);
    dvec2_mult_to (&force_p, (ang_b-ang_a)/(ang_a+ang_b)*h);
    dvec2_add_to (&res[p], force_p);

    //if (p==0) {
    //    printf ("v: %d, (%d, %d, %d) = ", v, s_m, p, s_p);
    //    dvec2_print (&force_p);
    //}
}

// Computes the force between two points proportional to the difference in their
// distance and length. When adding the resulting vector to a and it's inverse
// to b it effectively moves points closer to being at distance length. The
// value h is a positive value that determines the strength of the force.
dvec2 spring_force_pts (dvec2 a, dvec2 b, double length, double h)
{
    dvec2 res;
    dvec2 force = dvec2_subs (b, a);
    double d = dvec2_norm (force);
    if (d > 0) {
        dvec2_normalize (&force);
        res = dvec2_mult (force, h*(d-length));
    } else {
        res = DVEC2(0,0);
    }
    return res;
}

dvec2 spring_force (dvec2 *points, int a, int b, double length, double h)
{
    dvec2 p = points[a];
    dvec2 p_next = points[b];
    return spring_force_pts (p, p_next, length, h);
}

void arrange_points_start (struct arrange_points_state_t *alg_st, order_type_t *ot, dvec2 *points)
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

    dvec2 cvx_hull_v2[len];
    dvec2_idx_to_array (points, alg_st->cvx_hull, cvx_hull_v2, alg_st->cvx_hull_len);
    alg_st->centroid = polygon_centroid (cvx_hull_v2, alg_st->cvx_hull_len);

    alg_st->tgt_hull = mem_pool_push_size(&alg_st->pool, sizeof(dvec2)*alg_st->cvx_hull_len);
    convex_point_set_radius (alg_st->cvx_hull_len, 0,
                             points[alg_st->cvx_hull[0]], alg_st->centroid,
                             alg_st->tgt_hull);
    alg_st->tgt_radius = dvec2_norm (dvec2_subs(points[alg_st->cvx_hull[0]], alg_st->centroid));
    alg_st->steps = 0;
}

// Returns true if the resulting point set is good enough to show to the user.
bool arrange_points_end (struct arrange_points_state_t *alg_st, dvec2 *points, int len)
{
    bool res = true;
    int i;
    for (i=0; i < len; i++) {
        dvec2_round (&points[i]);
    }

    box_t box;
    get_bounding_box (points, len, &box);

    for (i=0; i < len; i++) {
        dvec2_subs_to (&points[i], DVEC2(box.min.x, box.min.y));
    }

    ot_triples_t *trip_ot = ot_triples_new (alg_st->ot, &alg_st->pool);
    ot_triples_t *trip_pts = ps_triples_new (points, len, &alg_st->pool);
    if (!ot_triples_are_equal (trip_ot, trip_pts)) {
        res = false;
    }

    mem_pool_destroy(&alg_st->pool);
    return res;
}

double arrange_points_step (struct arrange_points_state_t *alg_st, dvec2 *points, int len)
{
    order_type_t *ot = alg_st->ot;
    alg_st->steps++;

    dvec2 res[len];
    int i;
    for (i=0; i<len; i++) {
        res[i] = (dvec2){0};
    }

    // Compute the sum of all angular forces on a point p
    dvec2 tmp_forces[len];
    for (i=0; i<len; i++) {
        tmp_forces[i] = (dvec2){0};
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
        dvec2_mult_to (&tmp_forces[i], 1);
        dvec2_add_to (&res[i], tmp_forces[i]);
    }

    // Circular wall
    for (i=0; i<alg_st->cvx_hull_len; i++) {
        dvec2 r = dvec2_subs (points[alg_st->cvx_hull[i]], alg_st->centroid);
        double dist = dvec2_norm(r) - alg_st->tgt_radius;
        if (dist > 0) {
            dvec2_normalize (&r);
            double norm = dvec2_dot (res[alg_st->cvx_hull[i]], r);
            dvec2_mult_to (&r, -(norm + 0.01*dist));
            dvec2_add_to (&res[alg_st->cvx_hull[i]], r);
        }
    }

    //for (i=0; i<len; i++) {
    //    dvec2_print (&res[i]);
    //}

    // Move the original points by the computed force and compute the change
    // indicator.
    double change = 0;
    dvec2 old_p_0 = points[0];
    dvec2_add_to (&points[0], res[0]);
    //dvec2_print (&res[0]);
    for (i=1; i<len; i++) {
        double old_dist = dvec2_distance (&old_p_0, &points[i]);
        dvec2_add_to (&points[i], res[i]);
        //dvec2_print (&res[i]);
        double new_dist = dvec2_distance (&points[0], &points[i]);
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

    dvec2 delta_canvas = gui_st->ptr_delta;
    apply_inverse_transform_distance (T, &delta_canvas);
    dvec2_add_to (&ps_mode->visible_pts[i], delta_canvas);

    dvec2_add_to (&hitbox->box.min, gui_st->ptr_delta);
    dvec2_add_to (&hitbox->box.max, gui_st->ptr_delta);
}

void move_hitbox_int (struct point_set_mode_t *ps_mode)
{
    struct gui_state_t *gui_st = global_gui_st;
    int i = ps_mode->active_hitbox;
    layout_box_t *hitbox = &ps_mode->pts_hitboxes[i];
    transf_t *T = &ps_mode->points_to_canvas;

    dvec2 ptr = gui_st->input.ptr;
    apply_inverse_transform (T, &ptr);
    dvec2_round (&ptr);

    ps_mode->visible_pts[i] = ptr;
    dvec2_add_to (&hitbox->box.min, gui_st->ptr_delta);
    dvec2_add_to (&hitbox->box.max, gui_st->ptr_delta);
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
    mem_pool_end_temporary_memory (global_gui_st->thread_mem_flush);
    pthread_exit(0);
}

void set_ot (struct point_set_mode_t *ps_mode)
{
    int_string_update (&ps_mode->ot_id, ps_mode->ot->id);

    ps_mode->view_db_ot = true;
    int n = ps_mode->n.i;
    int i;
    for (i=0; i<n; i++) {
        dvec2 pt = CAST_DVEC2(ps_mode->ot->pts[i]);
        ps_mode->visible_pts[i] = pt;
        ps_mode->arranged_pts[i] = pt;
    }

    if (global_gui_st->thread != 0) {
        pthread_join (global_gui_st->thread, NULL);
    }

    struct arrange_work_t *wk =
        mem_pool_push_size (&global_gui_st->thread_pool, sizeof(struct arrange_work_t));
    wk->ps_mode = ps_mode;
    wk->n = ps_mode->n.i;
    wk->curr_n = &ps_mode->n.i;
    wk->ot = order_type_copy (&global_gui_st->thread_pool, ps_mode->ot);

    ps_mode->ot_arrangeable = false;
    ps_mode->redraw_panel = true;

    global_gui_st->thread_mem_flush = mem_pool_begin_temporary_memory (&global_gui_st->thread_pool);
    pthread_create (&global_gui_st->thread, NULL, arrange_points_work, wk);
}

void set_k (struct point_set_mode_t *st, int k)
{
    int_string_update (&st->k, k);
    st->num_triangle_subsets = binomial(binomial(st->n.i,3),k);
}

void set_n (struct point_set_mode_t *st, int n, app_graphics_t *graphics)
{
    mem_pool_end_temporary_memory (st->math_memory_flush);

    int_string_update (&st->n, n);

    int initial_k = thrackle_size (n);
    if (initial_k == -1) {
        initial_k = thrackle_size_lower_bound (n);
    }

    st->ot = order_type_new (10, &st->math_memory);
    open_database (n);
    st->ot->n = n;
    db_next (st->ot);
    set_ot (st);

    st->triangle_it = subset_it_new (n, 3, &st->math_memory);
    subset_it_precompute (st->triangle_it);
    set_k (st, initial_k);
    focus_order_type (graphics, st);
}

bool update_button_state (struct app_state_t *st, layout_box_t *lay_box, bool *update_panel)
{
    bool retval = false;
    if (!(lay_box->active_selectors & CSS_SEL_DISABLED) &&
        st->gui_st.mouse_clicked[0] &&
        is_dvec2_in_box (global_gui_st->click_coord[0], lay_box->box) &&
        (lay_box->active_selectors & CSS_SEL_HOVER)) {
        retval = true;
    }
    return retval;
}

enum layout_mode_t {
    LAYOUT_MODE_UNINITIALIZED,
    LAYOUT_MODE_GIVEN_ABSOLUTE,
    LAYOUT_MODE_GIVEN_RELATIVE,
    LAYOUT_MODE_CONTENT_FIT
};

struct positioning_info_t {
    enum layout_mode_t mode;
    union {
        box_t absolute;
        struct {
            dvec2 position;
            dvec2 size;
        };

        struct {
            dvec2 anchor;
            dvec2 weight;
        };
    };
};

static inline
void layout_box_set_position (layout_box_t *lay, struct positioning_info_t pos_info)
{
    compute_content_size (lay);

    // TODO: Do we want to call the behaviors for boxes that use ABSOLUTE or
    // RELATIVE positioning? (those whose position is already resolved).
    switch (pos_info.mode) {
        case LAYOUT_MODE_GIVEN_ABSOLUTE:
            lay->box = pos_info.absolute;
            break;
        case LAYOUT_MODE_GIVEN_RELATIVE:
            if (pos_info.size.x > 0) {
                lay->box.min.x = pos_info.position.x;
                lay->box.max.x = pos_info.position.x + pos_info.size.x;
            } else {
                lay->box.min.x = pos_info.position.x + pos_info.size.x;
                lay->box.max.x = pos_info.position.x;
            }

            if (pos_info.size.y > 0) {
                lay->box.min.y = pos_info.position.y;
                lay->box.max.y = pos_info.position.y + pos_info.size.y;
            } else {
                lay->box.min.y = pos_info.position.y + pos_info.size.y;
                lay->box.max.y = pos_info.position.y;
            }
            break;
        case LAYOUT_MODE_CONTENT_FIT:
            {
                dvec2 layout_size = DVEC2 (lay->content.width, lay->content.height);
                css_cont_size_to_lay_size (lay->style, &layout_size);

                lay->box.min.x = pos_info.anchor.x - layout_size.x*pos_info.weight.x;
                lay->box.min.y = pos_info.anchor.y - layout_size.y*pos_info.weight.y;
                lay->box.max = dvec2_add (lay->box.min, layout_size);
            } break;
        case LAYOUT_MODE_UNINITIALIZED:
            break;
        default:
            invalid_code_path;
    }
}

#define POS(x,y) (struct positioning_info_t){LAYOUT_MODE_CONTENT_FIT,{{DVEC2(x,y),DVEC2(0,0)}}}
#define POS_CENTERED(x,y) (struct positioning_info_t){LAYOUT_MODE_CONTENT_FIT,{{DVEC2(x,y),DVEC2(0.5,0.5)}}}
#define POS_UNINITIALIZED (struct positioning_info_t){LAYOUT_MODE_UNINITIALIZED,{{DVEC2(0,0), DVEC2(0,0)}}}

layout_box_t*
label (char *str, struct positioning_info_t pos_info)
{
    layout_box_t *label = next_layout_box (CSS_LABEL);
    layout_set_content_str (label, str);
    layout_box_set_position (label, pos_info);
    return label;
}

layout_box_t*
title (char *str, struct positioning_info_t pos_info)
{
    layout_box_t *title_box = next_layout_box (CSS_TITLE_LABEL);
    layout_set_content_str (title_box, str);
    layout_box_set_position (title_box, pos_info);
    return title_box;
}

layout_box_t*
button (char *label, bool *target, struct positioning_info_t pos_info)
{
    layout_box_t *curr_box = next_layout_box (CSS_BUTTON);
    focus_chain_add (global_gui_st, curr_box);
    add_behavior (global_gui_st, curr_box, BEHAVIOR_BUTTON, target);

    layout_set_content_str (curr_box, label);
    layout_box_set_position (curr_box, pos_info);
    return curr_box;
}

layout_box_t*
text_entry (uint64_string_t *target, struct positioning_info_t pos_info)
{
    layout_box_t *curr_box = next_layout_box (CSS_TEXT_ENTRY);
    focus_chain_add (global_gui_st, curr_box);
    add_behavior (global_gui_st, curr_box, BEHAVIOR_TEXT_ENTRY, target);

    layout_set_content_str (curr_box, str_data (&target->str));
    layout_box_set_position (curr_box, pos_info);
    return curr_box;
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

layout_box_t* separator (double x, double y, double width)
{
    layout_box_t* curr_box = next_layout_box (CSS_NONE);
    BOX_X_Y_W_H(curr_box->box, x, y, width, 2);
    curr_box->draw = draw_separator;
    return curr_box;
}

struct labeled_layout_row_t {
    // NOTE: label == NULL means main_layout should be made full width.
    layout_box_t *label;
    layout_box_t *main_layout;
    double row_height;
    struct labeled_layout_row_t *next;
};

typedef struct {
    double y_step;
    double x_step;
    double y_pos;
    double x_pos;
    double y;
    double x;
    layout_box_t *container;
    mem_pool_t pool;
    struct labeled_layout_row_t *rows;
} labeled_entries_layout_t;

labeled_entries_layout_t
begin_labeled_layout (double x_pos, double y_pos,
                      double x_step, double y_step,
                      layout_box_t *container)
{
    labeled_entries_layout_t lay = {0};
    lay.x_pos = x_pos;
    lay.y_pos = y_pos;
    lay.container = container;
    container->box.min = DVEC2 (x_pos, y_pos);
    lay.x = x_pos + lay.container->style->padding_x;
    lay.y = y_pos + lay.container->style->padding_y;
    lay.x_step = x_step;
    lay.y_step = y_step;
    return lay;
}

struct labeled_layout_row_t* labeled_layout_new_row (labeled_entries_layout_t *lay)
{
    struct labeled_layout_row_t *new_row =
        mem_pool_push_size (&lay->pool, sizeof (struct labeled_layout_row_t));
    if (lay->rows == NULL) {
        new_row->next = new_row;
    } else {
        new_row->next = lay->rows->next;
        lay->rows->next = new_row;
    }
    lay->rows = new_row;
    return new_row;
}

layout_box_t* labeled_text_entry (labeled_entries_layout_t *lay,
                                  char * label_str,
                                  uint64_string_t *target,
                                  struct app_state_t *st)
{
    struct labeled_layout_row_t *row = labeled_layout_new_row (lay);

    row->label = label (label_str, POS_UNINITIALIZED);
    row->label->box.min = DVEC2 (lay->x, lay->y);

    row->label->text_align_override = CSS_TEXT_ALIGN_RIGHT;
    row->label->box.min = DVEC2 (lay->x, lay->y);

    double label_height = row->label->content.height;
    css_cont_size_to_lay_size_w_h (row->label->style, NULL, &label_height);

    row->main_layout = text_entry (target, POS_UNINITIALIZED);

    double text_entry_height = row->main_layout->content.height;
    css_cont_size_to_lay_size_w_h (row->main_layout->style, NULL, &text_entry_height);

    row->row_height = MAX (label_height, text_entry_height);

    lay->y += row->row_height + lay->y_step;
    return row->main_layout;
}

layout_box_t* labeled_button (labeled_entries_layout_t *lay,
                              char *label_str,
                              char *button_text, bool *target,
                              struct app_state_t *st)
{
    struct labeled_layout_row_t *row = labeled_layout_new_row (lay);

    row->label = label (label_str, POS_UNINITIALIZED);
    row->label->box.min = DVEC2 (lay->x, lay->y);

    row->label->text_align_override = CSS_TEXT_ALIGN_RIGHT;
    row->label->box.min = DVEC2 (lay->x, lay->y);

    double label_height = row->label->content.height;
    css_cont_size_to_lay_size_w_h (row->label->style, NULL, &label_height);

    row->main_layout = button (button_text, target, POS_UNINITIALIZED);

    double button_height = row->main_layout->content.height;
    css_cont_size_to_lay_size_w_h (row->main_layout->style, NULL, &button_height);

    row->row_height = MAX (label_height, button_height);

    lay->y += row->row_height + lay->y_step;
    return row->main_layout;
}

void labeled_layout_skip (labeled_entries_layout_t *lay, double y_skip)
{
    lay->y += y_skip + lay->y_step;
}

layout_box_t* labeled_layout_separator (labeled_entries_layout_t *lay, struct app_state_t *st)
{
    struct labeled_layout_row_t *row = labeled_layout_new_row (lay);

    row->label = NULL;

    row->main_layout = separator (lay->x, lay->y, NAN);
    row->row_height = BOX_HEIGHT (row->main_layout->box);
    labeled_layout_skip (lay, row->row_height);

    return row->main_layout;
}

void end_labeled_layout (labeled_entries_layout_t *lay,
                         double align_pos,
                         struct app_state_t *st)
{
    struct labeled_layout_row_t *curr_row = lay->rows->next;
    double max_label_w = 0, max_main_w = 0;
    do {
        if (curr_row->label != NULL) {
            layout_box_t *label = curr_row->label;
            max_label_w = MAX (max_label_w, label->content.width);

            layout_box_t *main_box = curr_row->main_layout;

            double curr_main_width = main_box->content.width;
            css_cont_size_to_lay_size_w_h (main_box->style, &curr_main_width, NULL);
            max_main_w = MAX (max_main_w, curr_main_width);
        }

        curr_row = curr_row->next;
    } while (curr_row != lay->rows->next);

    // NOTE: We do this outside of the previuos loop because we know all labels
    // have the same style.
    double label_width = max_label_w;
    css_cont_size_to_lay_size_w_h (curr_row->label->style, &label_width, NULL);

    double main_width = MAX (max_main_w, (1-align_pos)*(max_label_w/2+lay->x_step/(align_pos*2)));

    double align_x = lay->x + label_width + lay->x_step;

    curr_row = lay->rows->next;
    do {
        if (curr_row->label != NULL) {
            layout_box_t *label = curr_row->label;
            label->box.max.x = lay->x + label_width;
            label->box.max.y = label->box.min.y + curr_row->row_height;

            layout_box_t *main_box = curr_row->main_layout;
            main_box->box.min.x = align_x;
            main_box->box.min.y = label->box.min.y;
            main_box->box.max.x = align_x + main_width;
            main_box->box.max.y = label->box.min.y + curr_row->row_height;
        } else {
            layout_box_t *main_box = curr_row->main_layout;
            main_box->box.max.x = align_x + main_width;
        }

        curr_row = curr_row->next;
    } while (curr_row != lay->rows->next);

    lay->container->box.max.y = lay->y - lay->y_step + lay->container->style->padding_y;
    lay->container->box.max.x = align_x + main_width + lay->container->style->padding_x;
    mem_pool_destroy (&lay->pool);
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

void view_arranged_points (struct point_set_mode_t *ps_mode, app_graphics_t *graphics)
{
    if (ps_mode->ot_arrangeable) {
        int i;
        for (i=0; i<ps_mode->n.i; i++) {
            ps_mode->visible_pts[i] = ps_mode->arranged_pts[i];
        }
        focus_order_type (graphics, ps_mode);
        ps_mode->redraw_canvas = true;
    }
}

void view_database_points (struct point_set_mode_t *ps_mode, app_graphics_t *graphics)
{
    if (ps_mode->ot_arrangeable) {
        int i;
        for (i=0; i<ps_mode->n.i; i++) {
            ps_mode->visible_pts[i] = CAST_DVEC2(ps_mode->ot->pts[i]);
        }
        focus_order_type (graphics, ps_mode);
        ps_mode->redraw_canvas = true;
    }
}

bool keycode_is_digit(xcb_keycode_t  keycode)
{
    switch(keycode){
        case KEY_1:
        case KEY_2:
        case KEY_3:
        case KEY_4:
        case KEY_5:
        case KEY_6:
        case KEY_7:
        case KEY_8:
        case KEY_9:
        case KEY_0:
            return true;
        default:
            return false;
    }
}

int keycode_get_digit(xcb_keycode_t  keycode)
{
    switch(keycode){
        case KEY_1:
            return 1;
        case KEY_2:
            return 2;
        case KEY_3:
            return 3;
        case KEY_4:
            return 4;
        case KEY_5:
            return 5;
        case KEY_6:
            return 6;
        case KEY_7:
            return 7;
        case KEY_8:
            return 8;
        case KEY_9:
            return 9;
        case KEY_0:
            return 0;
        default:
            return -1;
    }
}

bool point_set_mode (struct app_state_t *st, app_graphics_t *graphics)
{
    struct point_set_mode_t *ps_mode = st->ps_mode;
    struct gui_state_t *gui_st = &st->gui_st;
    if (!ps_mode) {
        st->ps_mode =
            mem_pool_push_size_full (&st->memory, sizeof(struct point_set_mode_t), POOL_ZERO_INIT, NULL, NULL);
        ps_mode = st->ps_mode;
        ps_mode->math_memory_flush = mem_pool_begin_temporary_memory (&ps_mode->math_memory);

        if (in_array (8, st->config->n_values, st->config->num_n_values)) {
            int_string_update (&ps_mode->n, 8);
        } else {
            int_string_update (&ps_mode->n, st->config->n_values[0]);
        }
        int_string_update (&ps_mode->k, thrackle_size (ps_mode->n.i));
        int_string_update (&ps_mode->ts_id, 0);

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
    }

    app_input_t input = st->gui_st.input;
    switch (input.keycode) {
        case KEY_P:
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
        case KEY_DOWN_ARROW:
        case KEY_UP_ARROW:
            break;
        case KEY_TAB:
            if (XCB_KEY_BUT_MASK_SHIFT & input.modifiers) {
                focus_prev (gui_st);
            } else {
                focus_next (gui_st);
            }
            ps_mode->redraw_panel = true;
            break;
        case KEY_N:
        case KEY_LEFT_ARROW:
        case KEY_RIGHT_ARROW:
            {
            layout_box_t *focused_box = gui_st->focus->dest;
            if (ps_mode->focus_list[foc_ot] == focused_box) {
                if ((XCB_KEY_BUT_MASK_SHIFT & input.modifiers)
                    || input.keycode == KEY_LEFT_ARROW) {
                    db_prev (ps_mode->ot);

                } else {
                    db_next (ps_mode->ot);
                }
                set_ot (ps_mode);
                focus_order_type (graphics, ps_mode);

            } else if (ps_mode->focus_list[foc_n] == focused_box) {
                int n = ps_mode->n.i;
                int n_pos = 0;
                while (st->config->n_values[n_pos] != n &&
                       n_pos < st->config->num_n_values) {
                    n_pos++;
                }

                if ((XCB_KEY_BUT_MASK_SHIFT & input.modifiers)
                        || input.keycode == KEY_LEFT_ARROW) {
                    if (n_pos == 0) {
                        n_pos = st->config->num_n_values-1;
                    } else {
                        n_pos--;
                    }
                } else {
                    if (n_pos == st->config->num_n_values-1) {
                        n_pos = 0;
                    } else {
                        n_pos++;
                    }
                }
                set_n (ps_mode, st->config->n_values[n_pos], graphics);
            } else if (ps_mode->focus_list[foc_k] == focused_box) {
                uint64_t max_k = ps_mode->triangle_it->size;
                if ((XCB_KEY_BUT_MASK_SHIFT & input.modifiers)
                        || input.keycode == KEY_LEFT_ARROW) {
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
                if ((XCB_KEY_BUT_MASK_SHIFT & input.modifiers)
                        || input.keycode == KEY_LEFT_ARROW) {
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
        case KEY_C:
            if (XCB_KEY_BUT_MASK_CONTROL & input.modifiers) {
                if (gui_st->selection.dest != NULL) {
                    gui_st->set_clipboard_str (gui_st->selection.start,
                                               gui_st->selection.len);
                }
            }
            break;
        case KEY_V:
            if (XCB_KEY_BUT_MASK_CONTROL & input.modifiers) {
                gui_st->get_clipboard_str ();
            } else {
                if (ps_mode->ot_arrangeable) {
                    ps_mode->view_db_ot = !ps_mode->view_db_ot;
                    if (ps_mode->view_db_ot) {
                        view_database_points (ps_mode, graphics);
                    } else {
                        view_arranged_points (ps_mode, graphics);
                    }
                    ps_mode->redraw_canvas = true;
                }
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
        panel = next_layout_box (CSS_BACKGROUND);
        labeled_entries_layout_t lay =
            begin_labeled_layout (10, 10, 12, 12, panel);

        layout_box_t *tmp = title ("Point Set", POS(lay.x, lay.y));
        labeled_layout_skip (&lay, BOX_HEIGHT (tmp->box));

        ps_mode->focus_list[foc_n] =
            labeled_text_entry (&lay, "n:", &ps_mode->n, st);
        ps_mode->focus_list[foc_ot] =
            labeled_text_entry (&lay, "Order Type:", &ps_mode->ot_id, st);
        ps_mode->arrange_pts_btn =
            labeled_button (&lay, "Layout:", "Arrange", &ps_mode->arrange_pts,
                            st);
        focus_chain_remove (gui_st, ps_mode->arrange_pts_btn);

        labeled_layout_separator (&lay, st);

        tmp = title ("Triangle Set", POS(lay.x, lay.y));
        labeled_layout_skip (&lay, BOX_HEIGHT (tmp->box));

        ps_mode->focus_list[foc_k] = labeled_text_entry (&lay, "k:", &ps_mode->k, st);
        ps_mode->focus_list[foc_ts] = labeled_text_entry (&lay, "ID:", &ps_mode->ts_id,
                                                          st);
        end_labeled_layout (&lay, 0.2, st);
    }

    if (ps_mode->arrange_pts) {
        ps_mode->view_db_ot = false;
        view_arranged_points (ps_mode, graphics);
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

    update_layout_boxes (&st->gui_st, st->gui_st.layout_boxes,
                         st->gui_st.num_layout_boxes,
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
                                    select_all_str (gui_st, lay_box, lay_box->content.str);
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
                                } else if (KEY_BACKSPACE == input.keycode) {
                                    unselect (gui_st);
                                    int_string_clear (beh->target.u64_str);
                                    ps_mode->redraw_panel = true;

                                } else if (keycode_is_digit(input.keycode)) {
                                    unselect (gui_st);

                                    str_cpy (&ps_mode->bak_number.str, &beh->target.u64_str->str);
                                    int digit = keycode_get_digit(input.keycode);
                                    int_string_update (beh->target.u64_str, digit);

                                    ps_mode->redraw_panel = true;
                                    beh->state = 2;

                                } else if (gui_st->clipboard_ready) {
                                    unselect (gui_st);
                                    gui_st->clipboard_ready = false;
                                    ps_mode->redraw_panel = true;
                                    beh->state = 2;

                                } else if (KEY_ENTER == input.keycode ||
                                            KEY_ESC == input.keycode ||
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
                                    select_all_str (gui_st, lay_box, lay_box->content.str);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 1;

                                } else if (KEY_BACKSPACE == input.keycode) {
                                    unselect (gui_st);
                                    int_string_delete_digit (beh->target.u64_str);
                                    ps_mode->redraw_panel = true;

                                } else if (keycode_is_digit(input.keycode)) {
                                    int digit = keycode_get_digit(input.keycode);
                                    int_string_append_digit (beh->target.u64_str, digit);
                                    ps_mode->redraw_panel = true;

                                } else if (KEY_ENTER == input.keycode ||
                                           !(lay_box->active_selectors & CSS_SEL_FOCUS)) {
                                    lay_box->content_changed = true;
                                    lay_box->content.str = str_data (&beh->target.u64_str->str);
                                    ps_mode->redraw_panel = true;
                                    beh->state = 0;

                                } else if (KEY_ESC == input.keycode) {
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
                    bool is_valid = false;

                    int *valid_n = st->config->n_values;
                    uint32_t valid_n_size = st->config->num_n_values;
                    uint32_t len = valid_n_size;
                    while (len > 0) {
                        len--;
                        if (val == valid_n[len]) {
                            is_valid = true;
                        }
                    }

                    if (is_valid) {
                        set_n (ps_mode, val, graphics);
                    } else {
                        printf ("Tried to set an invalid value for n."
                                " Either not in range, or not configured in ~/.ps_viewer/viewer.conf\n");
                    }

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
                if (!is_dvec2_in_box (gui_st->click_coord[0], panel->box) && gui_st->input.mouse_down[0]) {
                    bool hit = false;
                    int i;
                    for (i=0; i<ps_mode->n.i; i++) {
                        layout_box_t *hitbox = &ps_mode->pts_hitboxes[i];

                        if (is_dvec2_in_box (gui_st->click_coord[0], hitbox->box)) {
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
    }

    bool blit_needed = false;
    cairo_t *cr = graphics->cr;
    if (input.force_redraw || ps_mode->redraw_canvas) {
        cairo_clear (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_paint (cr);

        if (ps_mode->zoom_changed) {
            dvec2 window_ptr_pos = input.ptr;
            apply_inverse_transform (&ps_mode->points_to_canvas, &window_ptr_pos);
            compute_transform (&ps_mode->points_to_canvas, input.ptr, window_ptr_pos, ps_mode->zoom);
        }

        // Points may have changed position suddenly, update the hitboxes.
        int i;
        for (i=0; i<ARRAY_SIZE(ps_mode->pts_hitboxes); i++) {
            layout_box_t *curr_hitbox = &ps_mode->pts_hitboxes[i];

            dvec2 point = ps_mode->visible_pts[i];
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
            dvec3 color;
            get_next_color (&color);
            triangle_entity_add (ps_mode, triangles[i], color);
        }

        draw_entities (ps_mode, graphics);

        ps_mode->redraw_panel = true;
        blit_needed = true;
    }

    if (ps_mode->redraw_panel) {
        int i;
        for (i=0; i<st->gui_st.num_layout_boxes; i++) {
            struct css_box_t *style = st->gui_st.layout_boxes[i].style;
            layout_box_t *layout = &st->gui_st.layout_boxes[i];
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

    layout_boxes_end_frame (st->gui_st.layout_boxes, st->gui_st.num_layout_boxes);
    layout_boxes_end_frame (ps_mode->pts_hitboxes, ps_mode->n.i);

    ps_mode->rebuild_panel = false;
    ps_mode->redraw_panel = false;
    ps_mode->redraw_canvas = false;

    if (st->end_execution) {
        mem_pool_destroy (&ps_mode->math_memory);
    }

    return blit_needed;
}
