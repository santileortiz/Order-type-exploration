/* 
 * Copiright (C) 2017 Santiago León O.
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo-pdf.h>

#include "common.h"
#include "slo_timers.h"
#include "ot_db.h"
#include "geometry_combinatorics.h"

#include "tree_layout.c"

// TODO: This function is duplicated! Fix all .h dependency issues and get files
// properly organized.
order_type_t* order_type_from_id (int n, uint64_t ot_id)
{
    order_type_t *res = order_type_new (n, NULL);
    if (ot_id == 0 && n>10) {
        convex_ot_searchable (res);
    } else {
        assert (n<=10);
        open_database (n);
        db_seek (res, ot_id);
    }
    return res;
}

int main ()
{
    mem_pool_t pool = {0};
    int n = 8, ot_id = 0;

    order_type_t *ot = order_type_from_id (n, ot_id);
    struct sequence_store_t seq = new_sequence_store (NULL, &pool);

    seq_set_seq_number (&seq, 1);
    seq_set_seq_len (&seq, 8);
    thrackle_search_tree (n, ot, &seq);
    backtrack_node_t *bt_root = seq_tree_end (&seq);
    //backtrack_tree_preorder_print(bt_root);

    double h_separation = 1.2, v_separation = 400;
    box_t bnd_box;
    layout_tree_node_t *root = create_layout_tree (&pool, h_separation, bt_root);

    tree_layout_first_walk (root);
    tree_layout_second_walk (root, v_separation, &bnd_box);
    //layout_tree_preorder_print (root);
    //printf ("\n");
    
    bnd_box.min.x -= 10;
    bnd_box.min.y -= 10;
    bnd_box.max.x += 10;
    bnd_box.max.y += 10;
    cairo_surface_t *surface = cairo_pdf_surface_create ("./out/n_8_thr_full.pdf",
                                                         BOX_WIDTH(bnd_box), BOX_HEIGHT(bnd_box));
    cairo_t *cr = cairo_create (surface);
    cairo_set_source_rgb (cr,1,1,1);
    cairo_paint (cr);
    cairo_set_source_rgb (cr,0,0,0);
    cairo_set_line_width (cr, 6);
    draw_view_tree_preorder (cr, root, -bnd_box.min.x, 5, 0.0);

    cairo_surface_flush (surface);
    cairo_status_t retval = cairo_surface_write_to_png (surface, "./out/n_8_thr_full.png");
    if (retval != CAIRO_STATUS_SUCCESS) {
        printf ("Error: %s\n", cairo_status_to_string (retval));
    }

    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    mem_pool_destroy (&pool);
    return 0;
}
