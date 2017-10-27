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

char* add_extension (mem_pool_t *pool, char *path, char *new_ext)
{
    size_t path_len = strlen(path);
    char *res = mem_pool_push_size (pool, path_len+strlen(new_ext)+2);
    strcpy (res, path);
    res[path_len++] = '.';
    strcpy (&res[path_len], new_ext);
    return res;
}

// NOTE: Returns a pointer INTO _path_ after the last '.' or NULL if it does not
// have an extension. Hidden files are NOT extensions (/home/.bashrc returns NULL).
char* get_extension (char *path)
{
    size_t path_len = strlen(path);
    int i=path_len-1;
    while (i>=0 && path[i] != '.' && path[i] != '/') {
        i--;
    }

    if (i == -1 || path[i] == '/' || (path[i] == '.' && (i==0 || path[i-1] == '/'))) {
        return NULL;
    }

    return &path[i+1];
}

void seq_tree_draw (char* fname, struct sequence_store_t *stor, double width,
                    double ar, double line_width, double min_line_width, double node_r)
{
    mem_pool_t pool = {0};
    if (stor->tree_root == NULL) {
        seq_tree_end (stor);
        if (stor->tree_root == NULL) {
            printf ("Do not use SEQ_DRY_RUN to draw tree. Aborting.\n");
            return;
        }
    }

    char *png_fname = NULL, *pdf_fname = NULL;
    char *ext = get_extension (fname);
    if (ext == NULL) {
        pdf_fname = add_extension (&pool, fname, "pdf");
        png_fname = add_extension (&pool, fname, "png");
    } else {
        if (strncmp (ext, "png", 3) == 0) {
            png_fname = fname;
        } else if (strncmp (ext, "pdf", 3) == 0) {
            pdf_fname = fname;
        } else {
            printf ("Invalid file format, only pdf and png supported.\n");
            return;
        }
    }

    // Compute line width
    //  If min_line_width != 0, then we use it to stroke lines with a width
    //  according to the number of nodes in the level.
    //  Otherwies, all lines are stroked with line_with.
    if (min_line_width == 0) {
        min_line_width = line_width;
    }
    uint64_t *nodes_per_l = get_nodes_per_len (stor->tree_root, &pool, stor->final_max_len);
    double *line_widths = mem_pool_push_size (&pool, sizeof(double)*(stor->final_max_len+1));
    uint64_t min_num_nodes = UINT64_MAX; //Minumum number of nodes in a level excluding the root and las one.
    uint64_t max_num_nodes = 0;
    int i;
    for (i=1; i<stor->final_max_len; i++) {
        min_num_nodes = MIN(min_num_nodes, nodes_per_l[i]);
        max_num_nodes = MAX(max_num_nodes, nodes_per_l[i]);
    }

    double fact = ((line_width-min_line_width)/(double)(max_num_nodes-min_num_nodes));
    for (i=1; i<=stor->final_max_len; i++) {
        line_widths[i] = min_line_width+fact*(max_num_nodes - nodes_per_l[i]);
    }

    // Compute the actual tree layout
    double heigh = width/ar;
    double h_margin = width*0.05;
    double v_margin = heigh*0.05;
    double h_separation = 0.5, v_separation = (heigh-v_margin)/stor->final_max_len;

    box_t bnd_box;
    BOX_X_Y_W_H(bnd_box,0,0,0,0);

    layout_tree_node_t *root = create_layout_tree (&pool, h_separation, stor->tree_root);

    tree_layout_first_walk (root);
    tree_layout_second_walk (root, v_separation, &bnd_box);
    
    cairo_surface_t *surface;

    if (pdf_fname != NULL) {
        surface = cairo_pdf_surface_create (pdf_fname, width, heigh);
    } else {
        surface = cairo_pdf_surface_create_for_stream (NULL, NULL, width, heigh);
    }

    cairo_t *cr = cairo_create (surface);
    cairo_set_source_rgb (cr,1,1,1);
    cairo_paint (cr);
    cairo_set_source_rgb (cr,0,0,0);
    cairo_set_line_width (cr, line_width);

    //TODO: SHAME!! Should be using transforms instead.
    draw_view_tree_preorder (cr, root,
                             -bnd_box.min.x*(width-h_margin)/BOX_WIDTH(bnd_box)
                             + h_margin/2, (width-h_margin)/BOX_WIDTH(bnd_box),
                             v_margin/2, node_r, line_widths);

    cairo_surface_flush (surface);

    if (png_fname != NULL) {
        cairo_status_t retval = cairo_surface_write_to_png (surface, png_fname);
        if (retval != CAIRO_STATUS_SUCCESS) {
            printf ("Error: %s\n", cairo_status_to_string (retval));
        }
    }

    cairo_surface_destroy (surface);
    cairo_destroy (cr);
    mem_pool_destroy (&pool);
}

int main ()
{
    mem_pool_t pool = {0};
    int n = 6, ot_id = 0, k=10;

    int total_triangles = binomial(n,3);
    int *triangle_order = NULL;
    //switch (ord) {
    //    case TR_EDGE_LEXICOGRAPHIC:
    //     triangle_order = malloc (sizeof(int) * total_triangles);
    //     int j;
    //        for (j=0; j<total_triangles; j++) {
    //            triangle_order[j] = j;
    //        }
    //        lex_size_triangle_sort_user_data (triangle_order, total_triangles, &n);
    //        break;
    //    case TR_SIZE:
    //        triangle_order = malloc (sizeof(int) * total_triangles);
    //        order_triangles_size (n, triangle_order);
    //        break;
    //    default:
    //        triangle_order = NULL;
    //}

    if (triangle_order != NULL) {
        // NOTE: Reverse array
        int i;
        for (i=0; i<total_triangles/2; i++) {
            swap (&triangle_order[i], &triangle_order[total_triangles-i-1]);
        }
    }

    order_type_t *ot = order_type_from_id (n, ot_id);
    struct sequence_store_t seq = new_sequence_store (NULL, &pool);

    seq_set_seq_number (&seq, 100);
    //seq_set_seq_len (&seq, k);
    K_n_n_1_factorizations (n, NULL, &seq);
    //backtrack_tree_preorder_print(bt_root);

    seq_tree_draw ("./out/K_5_5_1-factorizations.png", &seq, 2000, 1.681, 2, 0, 3);

    mem_pool_destroy (&pool);
    return 0;
}
