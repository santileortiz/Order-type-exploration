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
    iterate_order_type,
    iterate_triangle_set,
    iterate_n,
    num_iterator_mode
} iterator_mode_t;

typedef struct {
    int i;
    char str[20];
} int_string_t;

struct point_set_mode_t {
    int_string_t n;
    int_string_t ot_id;
    order_type_t *ot;
    int_string_t k;
    int db;

    double max_zoom;
    double zoom;
    bool zoom_changed;
    double old_zoom;
    transf_t points_to_canvas;

    iterator_mode_t it_mode;
    int64_t user_number;

    subset_it_t *triangle_it;
    subset_it_t *triangle_set_it;

    int num_entities;
    entity_t entities[300];

    bool view_db_ot;
    vect2_t visible_pts[50];

    memory_stack_t memory;
};

struct app_state_t;
bool point_set_mode (struct app_state_t *st, app_graphics_t *graphics);
#define POINT_SET_MODE_H
#endif
