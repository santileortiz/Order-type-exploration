#if !defined(GRID_MODE_H)
struct grid_mode_state_t{
    int n;
    vect2_t *points;
    box_t points_bb;
    double point_radius;

    double canvas_x, canvas_y;
    bool loops_computed;
    uint64_t num_loops;
    int_dyn_arr_t edges_of_all_loops;

    memory_stack_t memory;
};

struct app_state_t;
bool grid_mode (struct app_state_t *st, app_graphics_t *gr);
#define GRID_MODE_H
#endif
