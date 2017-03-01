//gcc -O2 -Wall -g -o bin/search search.c -lm
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

#define N 9

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
    uint64_t id = 0;
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
        if (id==0) {
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
    int count = 0;
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
            array_print (triangle_set_it->idx, k);
            //subset_it_print (triangle_set_it);
            count++;
        }

        if (!subset_it_next (triangle_set_it)) {
            break;
        }
    }
    printf ("total count: %d\n", count);
    free (triangle_it);
    free (triangle_set_it);
}

void fast_edge_disjoint_sets (int n, int k)
{
    int *all_edj_sets = malloc(840*12*12*11*10*sizeof(int));

    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    order_type_t *ot = order_type_new (n, NULL);
    db_seek (ot, 0);

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_precompute (triangle_it);

    int num_found;
    generate_edge_disjoint_triangle_sets (n, k, all_edj_sets, &num_found);
    printf ("Sets found: %d\n", num_found);

    int i;
    while (!db_is_eof()) {
        int found_th = 0;
        for (i=0; i<num_found*k; i+=k) {
            int j;
            for (j=0; j<k; j++) {
                subset_it_seek (triangle_it, all_edj_sets[i+j]);
                curr_set->e[j].v[0] = ot->pts[triangle_it->idx[0]];
                curr_set->e[j].v[1] = ot->pts[triangle_it->idx[1]];
                curr_set->e[j].v[2] = ot->pts[triangle_it->idx[2]];
            }
            if (is_thrackle(curr_set)) {
                found_th++;
                //printf ("%"PRIu64": ", subset_it_id_for_idx (binomial(n,3), &all_edj_sets[i], k));
                //array_print (&all_edj_sets[i], k);
            }
        }
        printf ("%d %d\n", ot->id, found_th);
        db_next (ot);
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
    //print_edge_disjoint_sets (8, 8);
    //generate_edge_disjoint_triangle_sets (10, 13);
    //fast_edge_disjoint_sets (9, 10);

    int found_th;
    order_type_t *ot = order_type_new (N, NULL);
    db_seek (ot, 0);
    iterate_threackles_backtracking (N, 1, ot, NULL, &found_th);
    printf ("Thrackles found: %d", found_th);
}
