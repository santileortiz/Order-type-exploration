/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#include <inttypes.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>

//#define NDEBUG
#include <assert.h>

#include "slo_timers.h"
#include "geometry_combinatorics.h"

#define OT_DB_IMPLEMENTATION
#include "ot_db.h"

// TODO: Add timing:
//   - Compute ETA.
//   - If ETA is too short, do not print anything.
//   - Have a maximum update delay so that slow procedures don't seem to be
//     stalled until the first # is printed.
void progress_bar (float val, float total)
{
    float percent = (val/(total-1))*100;
    int length = 60;

    char str[length+30];

    static int prev = -1;
    static int idx;

    idx = length*percent/100;
    if (prev != idx) {
        prev = idx;
        int i;
        for (i=0; i<length; i++) {
            if (i < idx) {
                str[i] = '#';
            } else {
                str[i] = '-';
            }
        }
        str[i] = '\0';
        fprintf (stderr, "\r[%s] %.2f%%", str, percent);
        if (percent == 100) {
            fprintf (stderr, "\r\e[K");
        }
    }
}

#if 1
#define single_thrackle_func single_thrackle
#else
#define single_thrackle_func single_thrackle_slow
#endif
void get_thrackle_for_each_ot (int n, int k)
{
    uint64_t id = 0;
    order_type_t *ot = order_type_new (n, NULL);
    int *curr_set = malloc (sizeof(int)*k);

    triangle_set_t *triangle_set = malloc (triangle_set_size (k));
    triangle_set->k = k;

    open_database (n);
    db_seek (ot, id);

    int total_triangles = binomial (n, 3);
    float average = 0;
    int nodes = 0, searches = 0;
    int rand_arr[total_triangles];
    init_random_array (rand_arr, total_triangles);
    bool found = single_thrackle_func (n, k, ot, curr_set, &nodes, rand_arr);

    bool print_all = false;
    while (!db_is_eof ()) {
        if (print_all) {
            printf ("%ld: ", id);
            if (found) {
                array_print (curr_set, k);
            } else {
                printf ("None\n");
            }

            //printf ("%ld: %lu\n", id, subset_it_id_for_idx (total_triangles, curr_set, k));
        } else {
            if (!found) {
                printf ("%ld\n", id);
            }
            progress_bar (id, __g_db_data.num_order_types);
        }

        db_next (ot);
        triangle_set_from_ids (ot, n, curr_set, k, triangle_set);

        found = false;
        if (!is_thrackle(triangle_set)) {
            fisher_yates_shuffle (rand_arr, total_triangles);
            nodes = 0;
            found = single_thrackle_func (n, k, ot, curr_set, &nodes, rand_arr);
            average += nodes;
            searches++;
        } else {
            found = true;
        }
        id++;
    }
    printf ("Searches: %d, Average nodes: %f\n", searches, average/searches);
}

void search_full_tree_all_ot (int n)
{
    assert(n <= 10);
    mem_pool_t pool = {0};
    uint64_t id = 0;
    order_type_t *ot = order_type_new (n, NULL);

    open_database (n);
    db_seek (ot, id);

    float average = 0;
    int max_size = 0;

    while (!db_is_eof ()) {
        pool_temp_marker_t mrk = mem_pool_begin_temporary_memory (&pool);
        struct sequence_store_t seq = new_sequence_store_opts (NULL, &pool, SEQ_DRY_RUN);
        thrackle_search_tree (n, ot, &seq);
        seq_tree_end (&seq);

        average += seq.num_nodes;
        max_size = MAX(seq.final_max_len, max_size);

        progress_bar (id, __g_db_data.num_order_types);
        db_next (ot);
        id++;
        mem_pool_end_temporary_memory (mrk);
    }
    printf ("Max Size: %d, Average nodes: %f\n", max_size, average/(id));
}

uint32_t* load_uint32_from_text_file (char *filename, int *count)
{
    assert (count != NULL);
    *count = 0;
    FILE *f = fopen (filename, "r");
    char *str = NULL;
    size_t str_len = 0;
    while (getline (&str, &str_len, f) != -1) {
        if (isdigit (str[0])) {
            (*count)++;
        }
        free (str);
        str = NULL;
        str_len = 0;
    }

    uint32_t *res = malloc ((*count) * sizeof(uint32_t));
    if (res != NULL ) {
        int i = 0;
        str = NULL;
        str_len = 0;
        rewind (f);
        while (getline (&str, &str_len, f) != -1) {
            if (isdigit (str[0])) {
                res[i++] = (uint32_t)strtoul (str, NULL, 0);
            }
            free (str);
            str = NULL;
            str_len = 0;
        }
    } else {
        printf ("malloc() failed reading file '%s'\n", filename);
    }
    return res;
}

void max_thrackle_size_ot_file (int n, char *filename)
{
    assert(n <= 10);
    mem_pool_t pool = {0};
    order_type_t *ot = order_type_new (n, NULL);

    int num_ot_ids = 0;
    uint32_t *ot_ids = load_uint32_from_text_file (filename, &num_ot_ids);

    open_database (n);
    float average = 0;
    int i;
    for (i=0; i < num_ot_ids; i++) {
        db_seek (ot, ot_ids[i]);

        pool_temp_marker_t mrk = mem_pool_begin_temporary_memory (&pool);
        struct sequence_store_t seq = new_sequence_store_opts (NULL, &pool, SEQ_DRY_RUN);
        thrackle_search_tree (n, ot, &seq);
        seq_tree_end (&seq);

        progress_bar (i, num_ot_ids);
        average += seq.num_nodes;
        printf ("%u: %d\n", ot_ids[i], seq.final_max_len);

        mem_pool_end_temporary_memory (mrk);
    }
    printf ("Average nodes: %f\n", average/(num_ot_ids));
    free (ot_ids);
}

// This version does an exhaustive search over all binomial(binomial(n,3),k)
// possible sets. Only useful for checking the correctness of other
// implementations.
void count_thrackles (int n, int k)
{
    uint64_t id = 0;
    open_database (n);
    order_type_t *ot = order_type_new (n, NULL);
    triangle_set_t *curr_set = malloc (triangle_set_size (k));
    curr_set->k = k;

    db_seek (ot, id);

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
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
    open_database (n);
    order_type_t *ot_1 = order_type_new (n, NULL);
    db_seek (ot_1, ot_id_1);
    ot_triples_t *ot_triples_1 = ot_triples_new (ot_1, NULL);

    order_type_t *ot_2 = order_type_new (n, NULL);
    db_seek (ot_2, ot_id_2);
    ot_triples_t *ot_triples_2 = ot_triples_new (ot_2, NULL);

    int32_t trip_id = 0;
    while (trip_id < ot_triples_1->size) {
        if (ot_triples_1->triples[trip_id] != ot_triples_2->triples[trip_id]) {
            print_triple (n, trip_id);
        }
        trip_id++;
    }
}

void print_edge_disjoint_sets (int n, int k)
{
    int count = 0;
    order_type_t *ot = order_type_new (n, NULL);
    triangle_set_t *curr_set = malloc (triangle_set_size (k));
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
    triangle_set_t *curr_set = malloc (triangle_set_size (k));
    curr_set->k = k;

    order_type_t *ot = order_type_new (n, NULL);
    db_seek (ot, 0);

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_precompute (triangle_it);

    mem_pool_t pool = {0};
    struct sequence_store_t seq = new_sequence_store (NULL, &pool);
    generate_edge_disjoint_triangle_sets (n, k, &seq);
    int *all_edj_sets = seq_end (&seq);

    printf ("Sets found: %d\n", seq.num_sequences);

    int i;
    while (!db_is_eof()) {
        int found_th = 0;
        for (i=0; i<seq.num_sequences*k; i+=k) {
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
    mem_pool_destroy (&pool);
}

void get_thrackle_list_filename (char *s, int len, int n, int ot_id, int k)
{
    snprintf (s, len, ".cache/n_%d-ot_%d-th_all-k_%d.bin", n, ot_id, k);
}

void get_all_thrackles (int n, int k, uint32_t ot_id, char* filename)
{
    assert (n<=10);
    open_database (n);
    order_type_t *ot = order_type_new (n, NULL);
    db_seek (ot, ot_id);

    struct sequence_store_t seq = new_sequence_store (filename, NULL);
    all_thrackles (n, k, ot, &seq);
    seq_end (&seq);
}

int* get_all_thrackles_cached (int n, int k, uint32_t ot_id)
{
    char filename[200];
    get_thrackle_list_filename (filename, ARRAY_SIZE(filename), n, ot_id, k);

    int *res = seq_read_file (filename, NULL, NULL, NULL);
    if (res == NULL) {
        get_all_thrackles (n, k, ot_id, filename);
        res = seq_read_file (filename, NULL, NULL, NULL);
    }

    return res;
}

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

#define print_info(n,ot_id) print_info_order (n,ot_id,NULL)
void print_info_order (int n, uint64_t ot_id, int *triangle_order)
{
    order_type_t *ot = order_type_from_id (n, ot_id);
    mem_pool_t temp_pool = {0};
    struct sequence_store_t seq = new_sequence_store_opts (NULL, &temp_pool, SEQ_DRY_RUN);

    thrackle_search_tree_full (n, ot, &seq, triangle_order);
    seq_tree_end (&seq);

    uint32_t h = seq.final_max_len;
    printf ("Levels: %d + root\n", h);
    printf ("Nodes: %"PRIu64" + root\n", seq.num_nodes-1);
    if (seq.nodes_per_len != NULL) {
        printf ("Nodes per level: ");
        print_uint_array (seq.nodes_per_len, seq.final_max_len+1);
    }
    printf ("Tree size: %"PRIu64" bytes\n", seq.expected_tree_size);
    printf ("Max children: %u\n", seq.final_max_children);

    if (seq.time != 0) {
        printf ("Time: %f ms\n", seq.time);
    }
    mem_pool_destroy (&temp_pool);
}

void print_all_thrackles (int n, uint64_t ot_id, int *triangle_order)
{
    order_type_t *ot = order_type_from_id (n, ot_id);
    mem_pool_t temp_pool = {0};
    struct sequence_store_t seq;
    seq = new_sequence_store (NULL, &temp_pool);

    thrackle_search_tree_full (n, ot, &seq, triangle_order);
    backtrack_node_t *root = seq_tree_end (&seq);

    seq_tree_print_sequences_print (root, seq.final_max_len, sorted_array_print);

    mem_pool_destroy (&temp_pool);
}

void print_info_random_order (int n, uint64_t ot_id)
{
    int rand_arr[binomial(n,3)];
    init_random_array (rand_arr, ARRAY_SIZE(rand_arr));
    print_info_order (n, ot_id, rand_arr);
}

#define average_search_nodes_lexicographic(n) average_search_nodes(n, NULL)
void average_search_nodes (int n, int *triangle_order)
{
    mem_pool_t temp_pool = {0};
    // TODO: move order_type_new() to use a mem_pool_t instead of a
    // memory_stack_t.
    order_type_t *ot = mem_pool_push_size (&temp_pool, order_type_size(n));
    ot->n = n;
    ot->id = -1;

    assert (n<=10);
    open_database (n);
    uint32_t ot_id = 0;
    db_seek (ot, ot_id);

    float nodes = 0;

    while (!db_is_eof()) {
        pool_temp_marker_t mrk = mem_pool_begin_temporary_memory (&temp_pool);
        struct sequence_store_t seq = new_sequence_store (NULL, &temp_pool);
        thrackle_search_tree_full (n, ot, &seq, triangle_order);
        nodes += seq.num_nodes;
        //printf ("%"PRIu32": %"PRIu32"\n", ot->id, seq.num_nodes);
        seq_tree_end (&seq);
        db_next(ot);
        mem_pool_end_temporary_memory (mrk);
    }
    printf ("Average nodes: %f\n", nodes/((float)__g_db_data.num_order_types));
    mem_pool_destroy (&temp_pool);
}

void average_search_nodes_random (int n)
{
    int rand_arr[binomial(n,3)];
    init_random_array (rand_arr, ARRAY_SIZE(rand_arr));
    average_search_nodes (n, rand_arr);
}

int* get_all_thrackles_convex_position (int n, int k, int *num_found)
{
    char filename[200];
    get_thrackle_list_filename (filename, ARRAY_SIZE(filename), n, 0, k);

    struct file_header_t info;
    int *res = seq_read_file (filename, NULL, &info, NULL);
    if (res == NULL) {
        order_type_t *ot = order_type_new (n, NULL);
        if (n<=10) {
            open_database (n);
            db_seek (ot, 0);
        } else {
            convex_ot_searchable (ot);
        }

        struct sequence_store_t seq = new_sequence_store (filename, NULL);
        all_thrackles (n, k, ot, &seq);
        seq_end (&seq);
        res = seq_read_file (filename, NULL, &info, NULL);
    }
    *num_found = info.num_sequences;
    return res;
}

void print_triangle_sizes_for_thrackles_in_convex_position (int n)
{
    int k = thrackle_size (n);
    int num_found;
    int *thrackles = get_all_thrackles_convex_position (n, k, &num_found);
    int i;
    for (i=0; i<num_found*k; i+=k) {
        int *thrackle = &thrackles[i];

        //array_print (thrackle, k);

        int num_sizes = n/2-1;
        int sizes[num_sizes];
        array_clear (sizes, num_sizes);

        int j;
        for (j = 0; j<k; j++) {
            int t_id = thrackle[j];
            int t[3];
            subset_it_idx_for_id (t_id, n, t, 3);

            int dist[3];
            int d_a = 0;
            int d_b = 0;

            d_a = t[1] - t[0] - 1;
            d_b = n + t[0] - t[1] - 1;
            if (d_a <d_b) {
                dist [0] = d_a;
            } else {
                dist [0] = d_b;
            }

            d_a = t[2] - t[1] - 1;
            d_b = n + t[1] - t[2] - 1;
            if (d_a <d_b) {
                dist [1] = d_a;
            } else {
                dist [1] = d_b;
            }

            d_a = t[2] - t[0] - 1;
            d_b = n + t[0] - t[2] - 1;
            if (d_a <d_b) {
                dist [2] = d_a;
            } else {
                dist [2] = d_b;
            }

            //printf ("triangle: ");
            //array_print (t, 3);
            //printf ("distances: ");
            //array_print (dist, 3);

            int h;
            int max_distance = 0;
            for (h=0; h<3; h++) {
                if (dist[h] > max_distance) {
                    max_distance = dist[h];
                }
            }

            assert (max_distance != 0);
            //printf ("max_dist: %d\n", max_distance);
            sizes[max_distance - 1]++;
            //printf ("\n");
        }

        //int total_triangles = binomial (n, 3);
        //uint64_t choosen_set_id = subset_it_id_for_idx (total_triangles, thrackle, k);
        //if (sizes [0] == 0 && sizes[1] == 2 && sizes[2] == 7 && sizes[3] == 3) {
        //if (sizes [0] == 0 && sizes[1] == 0) {
        //if (sizes[3] == 5) {
            //printf ("%"PRIu64": ", choosen_set_id);
            array_print (sizes, num_sizes);
        //}
    }
}

int main ()
{
    ensure_full_database ();
    //get_thrackle_for_each_ot (10, 12);
    //count_thrackles (8);
    //print_differing_triples (n, 0, 1);
    //print_edge_disjoint_sets (8, 8);
    //generate_edge_disjoint_triangle_sets (10, 13);
    //fast_edge_disjoint_sets (9, 10);

    //mem_pool_t pool = {0};
    //struct sequence_store_t seq = new_tree (NULL, &pool);
    //matching_decompositions_over_K_n_n (6, NULL, &seq);
    //seq_tree_end (&seq);

    //int n = 10, k = 12;
    //order_type_t *ot = order_type_from_id (n, 403098);
    //int res[k];
    //int nodes;
    //bool found = single_thrackle (n, k, ot, res, &nodes, NULL);
    //if (!found) {
    //    printf ("None\n");
    //} else {
    //    array_print (res, k);
    //}

    //get_all_thrackles (9, 10, 0, NULL);
    //print_triangle_sizes_for_thrackles_in_convex_position (10);
    //print_info (10, 0);
    //average_search_nodes_lexicographic (8);
    //print_info_random_order (6, 0);
    max_thrackle_size_ot_file (10, "n_10_sin_thrackle_12.txt");
    
    //int n = 8;
    //int rand_arr[binomial(n,3)];
    //init_random_array (rand_arr, ARRAY_SIZE(rand_arr));
    //print_all_thrackles (n, 0, rand_arr);

    //search_full_tree_all_ot (7);

    //int count = count_2_regular_subgraphs_of_k_n_n (5);
    //printf ("Total: %d\n", count);
}
