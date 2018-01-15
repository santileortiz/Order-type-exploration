/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(POINT_SET_MODE_H)
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
    foc_none,
    foc_n,
    foc_ot,
    foc_k,
    foc_ts,

    num_focus_options
} focus_options_t;

struct arrange_points_state_t {
    mem_pool_t pool;
    order_type_t *ot;
    int *sort;
    int cvx_hull_len;
    int *cvx_hull;
    vect2_t centroid;
    vect2_t *tgt_hull;
    double tgt_radius;
    uint64_t steps;
};

struct point_set_mode_t {
    uint64_string_t n;
    uint64_string_t ot_id;
    order_type_t *ot;
    uint64_string_t k;
    uint64_t num_triangle_subsets; // = binomial(binomial(n,3),k)
    uint64_string_t ts_id;
    layout_box_t *focus_list[num_focus_options];

    bool rebuild_panel; // Computes the state of all layout boxes
    bool redraw_panel;  // Renders available layout boxes to screen
    bool redraw_canvas;
    bool editing_entry;
    bool arrange_pts;
    bool ot_arrangeable;
    layout_box_t *arrange_pts_btn;
    uint64_string_t bak_number;

    int db;

    double max_zoom;
    double zoom;
    bool zoom_changed;
    double old_zoom;
    transf_t points_to_canvas;

    subset_it_t *triangle_it;

    int num_entities;
    entity_t entities[300];

    bool view_db_ot;
    struct arrange_points_state_t alg_st;
    vect2_t visible_pts[15];
    vect2_t arranged_pts[15];
    double point_radius;
    layout_box_t pts_hitboxes[15];
    int active_hitbox;
    int canvas_state;

    mem_pool_t memory;
    mem_pool_t pool;
};

struct app_state_t;
bool point_set_mode (struct app_state_t *st, app_graphics_t *graphics);
#define POINT_SET_MODE_H
#endif
