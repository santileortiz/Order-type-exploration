/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(APP_API_H)

typedef struct {
    float time_elapsed_ms;

    bool force_redraw;

    xcb_keycode_t keycode;
    uint16_t modifiers;

    float wheel;
    char mouse_down[3];

    vect2_t ptr;
} app_input_t;

typedef enum {
    APP_POINT_SET_MODE,
    APP_GRID_MODE,
    APP_TREE_MODE,

    NUM_APP_MODES
} app_mode_t;

struct app_state_t {
    bool is_initialized;

    bool end_execution;
    app_mode_t app_mode;
    struct point_set_mode_t *ps_mode;
    struct grid_mode_state_t *grid_mode;
    struct tree_mode_state_t *tree_mode;

    app_input_t input;
    char dragging[3];
    vect2_t ptr_delta;
    vect2_t click_coord[3];
    bool mouse_clicked[3];
    bool mouse_double_clicked[3];
    float time_since_last_click[3];
    float double_click_time;
    float min_distance_for_drag;

    struct css_box_t css_styles[CSS_NUM_STYLES];
    int num_layout_boxes;
    int focused_layout_box;
    layout_box_t layout_boxes[30];

    mem_pool_t memory;

    // NOTE: This will be cleared at every frame start
    // TODO: Should this be inside st->memory?
    mem_pool_t temporary_memory;
};

#define APP_API_H
#endif
