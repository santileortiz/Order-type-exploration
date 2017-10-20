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

char* change_extension (mem_pool_t *pool, char *path, char *new_ext)
{
    size_t path_len = strlen(path);
    int i=path_len;
    while (i>0 && path[i-1] != '.') {
        i--;
    }
    char *res = mem_pool_push_size (pool, path_len+strlen(new_ext)+1);
    strcpy (res, path);
    strcpy (&res[i], new_ext);
    return res;
}

void seq_tree_draw (char* fname, struct sequence_store_t *stor, double width,
                    double ar, double line_width, double node_r)
{
    mem_pool_t pool = {0};
    if (stor->tree_root == NULL) {
        seq_tree_end (stor);
        if (stor->tree_root == NULL) {
            printf ("Do not use SEQ_DRY_RUN to draw tree. Aborting.\n");
            return;
        }
    }

    double heigh = width/ar;
    double h_margin = width*0.05;
    double v_margin = heigh*0.05;
    double h_separation = 0.5, v_separation = (heigh-v_margin)/stor->final_max_len;

    box_t bnd_box;
    BOX_X_Y_W_H(bnd_box,0,0,0,0);

    layout_tree_node_t *root = create_layout_tree (&pool, h_separation, stor->tree_root);

    tree_layout_first_walk (root);
    tree_layout_second_walk (root, v_separation, &bnd_box);
    //layout_tree_preorder_print (root);
    //printf ("\n");
    
    cairo_surface_t *surface = cairo_pdf_surface_create (fname,
                                                         width, heigh);
    cairo_t *cr = cairo_create (surface);
    cairo_set_source_rgb (cr,1,1,1);
    cairo_paint (cr);
    cairo_set_source_rgb (cr,0,0,0);
    cairo_set_line_width (cr, line_width);

    //TODO: SHAME!! Should be using transforms instead.
    draw_view_tree_preorder (cr, root,
                             -bnd_box.min.x*(width-h_margin)/BOX_WIDTH(bnd_box)
                             + h_margin/2, (width-h_margin)/BOX_WIDTH(bnd_box),
                             v_margin/2, node_r);

    cairo_surface_flush (surface);

#if 1
    char *png_fname = change_extension (&pool, fname, "png");
    cairo_status_t retval = cairo_surface_write_to_png (surface, png_fname);
    if (retval != CAIRO_STATUS_SUCCESS) {
        printf ("Error: %s\n", cairo_status_to_string (retval));
    }
#endif

    cairo_surface_destroy (surface);
    cairo_destroy (cr);
    mem_pool_destroy (&pool);
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
    //backtrack_tree_preorder_print(bt_root);

    seq_tree_draw ("./out/tree.pdf", &seq, 1000, 1.681, 1, 0);

    mem_pool_destroy (&pool);
    return 0;
}
