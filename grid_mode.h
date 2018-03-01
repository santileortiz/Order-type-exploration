/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(GRID_MODE_H)
struct grid_mode_state_t{
    int n;
    dvec2 *points;
    box_t points_bb;
    double point_radius;

    double canvas_x, canvas_y;
    bool loops_computed;
    uint64_t num_loops;
    int_dyn_arr_t edges_of_all_loops;

    mem_pool_t memory;
};

struct app_state_t;
bool grid_mode (struct app_state_t *st, app_graphics_t *gr);
#define GRID_MODE_H
#endif
