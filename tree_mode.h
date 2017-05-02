#if !defined(TREE_MODE_H)
struct tree_mode_state_t{
    vect2_t root_pos;

    int n;
    vect2_t *points;
    box_t points_bb;
    double point_radius;

    uint64_t num_nodes;

    int *all_perms;
    memory_stack_t memory;

    mem_pool_t pool;
};

struct app_state_t;
bool tree_mode (struct app_state_t *st, app_graphics_t *gr);
#define TREE_MODE_H
#endif
