/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#define cairo_BOX(cr,box) cairo_rectangle (cr, (box).min.x, (box).min.y, BOX_WIDTH(box), BOX_HEIGHT(box));
void draw_graph (cairo_t *cr, struct grid_mode_state_t *grid_st, int id, box_t *dest)
{
    dest->min.x += grid_st->point_radius;
    dest->min.y += grid_st->point_radius;
    dest->max.x -= grid_st->point_radius;
    dest->max.y -= grid_st->point_radius;
    transf_t graph_to_dest;

    box_t *src = &grid_st->points_bb;
    compute_best_fit_box_to_box_transform (&graph_to_dest, src, dest);

    cairo_set_source_rgb (cr,0,0,0);
    int i;
    for (i=0; i<2*grid_st->n; i++) {
        vect2_t p = grid_st->points[i];
        apply_transform (&graph_to_dest, &p);
        cairo_arc (cr, p.x, p.y, grid_st->point_radius, 0, 2*M_PI);
        cairo_fill (cr);
    }

    int line_width = 1;
    for (i=0; i<2*grid_st->n; i++) {
        int e[2];
        e[0] = grid_st->edges_of_all_loops.data[id*4*grid_st->n+2*i];
        e[1] = grid_st->edges_of_all_loops.data[id*4*grid_st->n+2*i+1];
        vect2_t p1 = grid_st->points[e[0]];
        apply_transform (&graph_to_dest, &p1);
        pixel_align_as_line (&p1, line_width);

        vect2_t p2 = grid_st->points[e[1]];
        apply_transform (&graph_to_dest, &p2);
        pixel_align_as_line (&p2, line_width);

        cairo_set_line_width (cr, 1);
        cairo_move_to (cr, p1.x, p1.y);
        cairo_line_to (cr, p2.x, p2.y);
        cairo_stroke (cr);
    }
}

bool grid_mode (struct app_state_t *st, app_graphics_t *gr)
{
    struct grid_mode_state_t *grid_mode = st->grid_mode;
    if (!st->grid_mode) {
        uint32_t panel_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, panel_storage);
        st->grid_mode = (struct grid_mode_state_t*)base;
        memory_stack_init (&st->grid_mode->memory,
                           panel_storage-sizeof(struct grid_mode_state_t),
                           base+sizeof(struct grid_mode_state_t));

        grid_mode = st->grid_mode;
        grid_mode->n = 3;
        grid_mode->loops_computed = false;
        grid_mode->points = push_array (&grid_mode->memory, 2*grid_mode->n, vect2_t);

        grid_mode->canvas_x = 10;
        grid_mode->canvas_y = 10;
        grid_mode->point_radius = 5;

        bipartite_points (grid_mode->n, grid_mode->points, &grid_mode->points_bb, 0);
    }

    app_input_t input = st->input;
    cairo_t *cr = gr->cr;
    if (st->dragging[0]) {
        grid_mode->canvas_x += st->ptr_delta.x;
        grid_mode->canvas_y += st->ptr_delta.y;
        input.force_redraw = true;
    }

    if (!grid_mode->loops_computed) {
        int_dyn_arr_init (&grid_mode->edges_of_all_loops, 60);
        grid_mode->num_loops = count_2_regular_subgraphs_of_k_n_n (grid_mode->n, &grid_mode->edges_of_all_loops);
        grid_mode->loops_computed = true;
    }

    if (input.force_redraw) {
        box_t dest;
        int dest_width = 40;
        int dest_height = (dest_width-2*grid_mode->point_radius)/BOX_AR(grid_mode->points_bb)+
            2*grid_mode->point_radius;

#if 0
        cairo_BOX (cr, *dest);
        cairo_set_source_rgba (cr, 0.2, 1, 0.4, 0.4);
        cairo_fill (cr);
#endif

        cairo_set_source_rgb (cr, 1,1,1);
        cairo_paint (cr);
        
        int i;
        int x_slot = 0, y_slot = 0;
        int x_step=10, y_step = 20;
        int num_x_slots = grid_mode->num_loops/binomial(grid_mode->n,2);
        for (i=0; i<grid_mode->num_loops; i++) {
            int x_pos = grid_mode->canvas_x+(dest_width+x_step)*x_slot;
            int y_pos = grid_mode->canvas_y+(dest_height+y_step)*y_slot;
            
            BOX_X_Y_W_H (dest, x_pos, y_pos, dest_width, dest_height);
            if (is_box_visible (&dest, gr)) {
                draw_graph (cr, grid_mode, i, &dest);
            }

            if (x_slot == num_x_slots-1) {
                x_slot = 0;
                y_slot++;
            } else {
                x_slot++;
            }
        }
        return true;
    }
    return false;
}

