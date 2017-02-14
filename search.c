//gcc -Wall -O2 -g -o bin/search search.c -lm
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
    uint64_t id = 2197;
    order_type_t *ot = order_type_new (N, NULL);
    triangle_set_t *curr_set = malloc (sizeof(triangle_set_t)+(k-1)*sizeof(triangle_t));
    curr_set->k = k;

    db_seek (ot, id);

    subset_it_t *triangle_it = subset_it_new (N, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    subset_it_seek (triangle_set_it, 313832179);

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
                //subset_it_print (it);
                //count++;
                //printf("%d\n", it->id);
                break;
            } else {
                if (!subset_it_next (triangle_set_it)) {
                    subset_it_seek (triangle_set_it, 0);
                }
            }
        }
        //printf ("%ld: %ld\n", id, count);
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

    subset_it_t *triangle_it = subset_it_new (N, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    while (!db_is_eof()) {
        db_next (ot);
        uint64_t count = 0;
        subset_it_seek (triangle_set_it, 2);
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

int main ()
{
    if (!open_database (N)){
        printf ("Could not open database\n");
        return -1;
    }

    //get_thrackle_for_each_ot (12);
    //count_thrackles (8);
    print_differing_triples (N, 0, 1);

}
