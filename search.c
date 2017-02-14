//gcc -Wall -g -o bin/search search.c -lm
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

//#define NDEBUG
#include <assert.h>

#include "slo_timers.h"
#include "geometry_combinatorics.h"
#include "ot_db.h"

#define N 8

void get_thrackle_for_each_ot (int k)
{
    uint64_t id = 0;
    order_type_t *ot = order_type_new (N, NULL);
    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    db_seek (ot, id);

    subset_it_t *triangle_it = subset_it_new (N, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    subset_it_seek (triangle_set_it, 20620000000);

    while (!db_is_eof ()) {
        db_next (ot);
        while (1) {
            int i;
            //subset_it_print (triangle_set_it);
            for (i=0; i<k; i++) {
                subset_it_seek (triangle_it, triangle_set_it->idx[i]);
                curr_set->e[i].v[0] = ot->pts[triangle_it->idx[0]];
                curr_set->e[i].v[1] = ot->pts[triangle_it->idx[1]];
                curr_set->e[i].v[2] = ot->pts[triangle_it->idx[2]];
            }

            if (is_thrackle(curr_set)) {
            //if (is_edge_disjoint_set(curr_set)) {
                break;
            } else {
                if (!subset_it_next (triangle_set_it)) {
                    subset_it_seek (triangle_set_it, 0);
                }
            }

            if (triangle_set_it->id % 1000000000 == 0) {
                printf ("Currently on Triangle set: %"PRIu64"\n ot:%"PRIu64"\n", triangle_set_it->id, id);
            }
        }
        printf ("%ld: %ld\n", id, triangle_set_it->id);
        //if (id==2) {
        //    return;
        //}
        id++;
    }
    free (triangle_it);
    free (triangle_set_it);
}

void count_thrackles (int k)
{
    uint64_t id = 8;
    order_type_t *ot = order_type_new (N, NULL);
    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    db_seek (ot, id);

    subset_it_t *triangle_it = subset_it_new (N, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    while (!db_is_eof()) {
        db_next (ot);
        uint64_t count = 0;
        subset_it_seek (triangle_set_it, 0);
        while (1) {
            //BEGIN_WALL_CLOCK;
            int i;
            for (i=0; i<k; i++) {
                subset_it_seek (triangle_it, triangle_set_it->idx[i]);
                //printf ("\t");
                //subset_it_print (triangle_it);
                curr_set->e[i].v[0] = ot->pts[triangle_it->idx[0]];
                curr_set->e[i].v[1] = ot->pts[triangle_it->idx[1]];
                curr_set->e[i].v[2] = ot->pts[triangle_it->idx[2]];
            }

            if (is_thrackle(curr_set)) {
            //if (is_edge_disjoint_set(curr_set)) {
                //printf ("\t\t");
                subset_it_print (triangle_set_it);
                count++;
            }
            if (!subset_it_next (triangle_set_it)){
                break;
            }
            //PROBE_WALL_CLOCK("Triangle set loop");
        }
        printf ("%ld: %ld\n", id, count);
        if (id==9) {
            return;
        }
        id++;
    }
    free (triangle_it);
    free (triangle_set_it);
}

void print_differing_triples (int n, uint64_t ot_id_1, uint64_t ot_id_2)
{
    order_type_t *ot_1 = order_type_new (N, NULL);
    db_seek (ot_1, ot_id_1);
    ot_triples_t *ot_triples_1 = ot_triples_new (ot_1, NULL);

    order_type_t *ot_2 = order_type_new (N, NULL);
    db_seek (ot_2, ot_id_2);
    ot_triples_t *ot_triples_2 = ot_triples_new (ot_2, NULL);

    int32_t trip_id = 0;
    while (trip_id < ot_triples_1->size) {
        if (ot_triples_1->triples[trip_id] != ot_triples_2->triples[trip_id]) {
            print_triple (N, trip_id);
        }
        trip_id++;
    }
}

void print_edge_disjoint_sets (int n, int k)
{
    order_type_t *ot = order_type_new (n, NULL);
    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    db_seek (ot, 0);
    while (1) {
        int i;
        for (i=0; i<k; i++) {
            subset_it_seek (triangle_it, triangle_set_it->idx[i]);
            curr_set->e[i].v[0] = ot->pts[triangle_it->idx[0]];
            curr_set->e[i].v[1] = ot->pts[triangle_it->idx[1]];
            curr_set->e[i].v[2] = ot->pts[triangle_it->idx[2]];
        }

        if (is_edge_disjoint_set(curr_set)) {
            subset_it_print (triangle_set_it);
        }

        if (!subset_it_next (triangle_set_it)) {
            break;
        }
    }
    free (triangle_it);
    free (triangle_set_it);
}

bool in_array (int i, int* arr, int size)
{
    while (size) {
        size--;
        if (arr[size] == i) {
            return true;
        }
    }
    return false;
}

void swap (int*a, int*b)
{
    *a = *a^*b;
    *b = *a^*b;
    *a = *a^*b;
}

// This function was tested with this code:
//
// int n = 5;
// subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
// subset_it_precompute (triangle_it);
// do {
//     if (triangle_it->id != subset_it_id_for_idx (n, triangle_it->idx, 3)) {
//         printf ("There is an error\n");
//         subset_it_print (triangle_it);
//         break;
//     }
// } while (subset_it_next (triangle_it));

// TODO: What is the complexity of this? O(sum(idx)), maybe?
uint64_t subset_it_id_for_idx (int n, int *idx, int k)
{
    //TODO: use a generic sort here so it works for any k.
    assert (k==3);
    if (idx[0] > idx[1]) swap(&idx[0],&idx[1]);
    if (idx[1] > idx[2]) swap(&idx[1],&idx[2]);
    if (idx[0] > idx[1]) swap(&idx[0],&idx[1]);

    uint64_t id = 0;
    int idx_counter = 0;
    int h=0;
    while (h < k) {
        uint64_t subsets = binomial ((n)-(idx_counter)-1, k-h-1);
        if (idx_counter < idx[h]) {
            id+=subsets;
        } else {
            h++;
        }
        idx_counter++;
    }
    return id;
}

void fast_edge_disjoint_sets (int n, int k)
{
    uint64_t count = 0;
    order_type_t *ot = order_type_new (n, NULL);
    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);

    subset_it_precompute (triangle_it);

    int triangle_id = 0;
    int invalid_triangles[triangle_it->size];
    int num_invalid = 0;

    int choosen_triangles[k];
    int invalid_restore_indx[k];
    int triangles_remaining_for_choosen[k];
    int num_choosen = 0;
    db_seek (ot, 0);
    //triangles_remaining_for_choosen[0] = 0;
    triangles_remaining_for_choosen[0] = triangle_it->size-1;

    while (1) {
        choosen_triangles[num_choosen] = triangle_id;
        invalid_restore_indx[num_choosen] = num_invalid;

        num_choosen++;

        if (num_choosen == k) {
            count++;
            //if (count%10000 == 0) {
            //    printf ("count: %"PRIu64"\n", count);
            //}
            int p_k;
            for (p_k=0; p_k<k-1; p_k++) {
                printf ("%d ", choosen_triangles[p_k]);
            }
            printf ("%d\n", choosen_triangles[p_k]);
        }

        subset_it_seek (triangle_it, triangle_id);
        int triangle[3];
        triangle[0] = triangle_it->idx[0];
        triangle[1] = triangle_it->idx[1];
        triangle[2] = triangle_it->idx[2];

        int i;
        for (i=0; i<ot->n; i++) {
            int invalid_triangle[3];

            if (in_array(i, triangle, 3)) {
                    continue;
            }

            uint64_t id;
            int t;
            for (t=0; t<3; t++) {
                invalid_triangle[0] = i;
                invalid_triangle[1] = triangle[t];
                invalid_triangle[2] = triangle[(t+1)%3];
                id = subset_it_id_for_idx (n, invalid_triangle, 3);

                int j;
                for (j=0; j<num_invalid; j++) {
                    if (invalid_triangles[j] == id) {
                        break;
                    }
                }
                if (j == num_invalid) {
                    invalid_triangles[num_invalid] = id;
                    num_invalid++;
                }
            }
        }

        if (triangle_it->size == num_invalid+num_choosen) {
            // Backtrack
            do {
                num_choosen--;
                num_invalid = invalid_restore_indx[num_choosen];
                triangles_remaining_for_choosen[num_choosen]--;
                if (num_choosen == 0 && triangles_remaining_for_choosen[num_choosen] == -1) {
                    printf ("total count: %"PRIu64"\n", count);
                    return;
                }
            } while (triangles_remaining_for_choosen[num_choosen]+1 == 0);
            triangle_id = choosen_triangles[num_choosen]+1;
        } else {
            triangles_remaining_for_choosen[num_choosen] = triangle_it->size-num_invalid-num_choosen-1;
            triangle_id = choosen_triangles[num_choosen-1]+1;
        }

        while (in_array(triangle_id, invalid_triangles, num_invalid) ||
                in_array(triangle_id, choosen_triangles, num_choosen)) {
            triangle_id++;
            //triangle_id %=triangle_it->size;
        }

        if (triangle_id >= triangle_it->size) {
            do {
                num_choosen--;
                num_invalid = invalid_restore_indx[num_choosen];
                triangles_remaining_for_choosen[num_choosen]--;
                if (num_choosen == 0 && triangles_remaining_for_choosen[num_choosen] == -1) {
                    printf ("total count: %"PRIu64"\n", count);
                    return;
                }
            } while (triangles_remaining_for_choosen[num_choosen]+1 == 0);
            triangle_id = choosen_triangles[num_choosen]+1;
            while (in_array(triangle_id, invalid_triangles, num_invalid) ||
                    in_array(triangle_id, choosen_triangles, num_choosen)) {
                triangle_id++;
            }

        }
    }
    free (triangle_it);
}

int main ()
{
    if (!open_database (N)){
        printf ("Could not open database\n");
        return -1;
    }

    //get_thrackle_for_each_ot (12);
    //count_thrackles (8);
    //print_differing_triples (N, 0, 1);
    //print_edge_disjoint_sets (9, 12);
    fast_edge_disjoint_sets (9, 12);

}
