/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(TREE_MODE_H)
struct tree_mode_state_t{
    dvec2 root_pos;

    int n;
    dvec2 *points;
    box_t points_bb;
    double point_radius;

    uint64_t num_nodes;

    int *all_perms;

    mem_pool_t pool;
};

struct app_state_t;
bool tree_mode (struct app_state_t *st, app_graphics_t *gr);
#define TREE_MODE_H
#endif
