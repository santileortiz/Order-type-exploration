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
#include <errno.h>

#include "common.h"
#include "slo_timers.h"

#include "order_types.h"
#include "geometry_combinatorics.h"

#define SEQUENCE_STORE_IMPL
#include "sequence_store.h"

#include "order_types.c"

int main ()
{
    mem_pool_t pool = {0};
    int n = 8, ot_id = 0, k=10;

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

    //seq_set_seq_number (&seq, 1);
    //seq_set_seq_len (&seq, k);
    thrackle_search_tree_full (n, ot, &seq, triangle_order);
    //backtrack_tree_preorder_print(bt_root);

    seq_tree_draw ("./out/n_8_thr_full.png", &seq, 3000, 1.681, 2, 0, 0);

    mem_pool_destroy (&pool);
    return 0;
}
