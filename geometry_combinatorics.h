#if !defined(GEOMETRY_COMBINATORICS_H)
#include <math.h>
#include <time.h>
#include <string.h>
#include "common.h"

// TODO: These are 128 bit structs, they may take up a lot of space, maybe have
// another one of 32 bits for small coordinates.
// TODO: Also, how does this affect performance?
typedef union {
    struct {
        int64_t x;
        int64_t y;
    };
    int64_t E[2];
} vect2i_t;
#define VECT2i(x,y) ((vect2i_t){{x,y}})

typedef struct {
    int n;
    vect2_t pts [1];
} real_point_set_t;

typedef struct {
    int n;
    uint32_t id;
    vect2i_t pts [1];
} order_type_t;

#define order_type_size(n) (sizeof(order_type_t)+(n-1)*sizeof(vect2i_t))
order_type_t *order_type_new (int n, memory_stack_t *stack)
{
    order_type_t *ret;
    if (stack) {
         ret = push_size (stack, order_type_size(n));
    } else {
        ret = malloc(order_type_size(n));
    }
    ret->n = n;
    ret->id = -1;
    return ret;
}

#define convex_ot(ot) convex_ot_scale (ot, 255/2)
void convex_ot_scale (order_type_t *ot, int radius)
{
    assert (ot->n >= 3);
    double theta = (2*M_PI)/ot->n;
    ot->id = 0;
    int i;
    for (i=1; i<=ot->n; i++) {
        ot->pts[i-1].x = (int)((double)radius*(1+cos(i*theta)));
        ot->pts[i-1].y = (int)((double)radius*(1+sin(i*theta)));
    }
}

void bipartite_points (int n, vect2_t *points, box_t *bounding_box, double bb_ar)
{
    int64_t y_step = UINT8_MAX/(n-1);

    int64_t x_step;
    if (bb_ar == 0) {
        x_step = y_step;
    } else {
        x_step = UINT8_MAX/bb_ar;
    }
    int64_t y_pos = 0;
    int i;
    for (i=0; i<n; i++) {
        points[i].x = 0;
        points[i].y = y_pos;

        points[i+n].x = x_step;
        points[i+n].y = y_pos;
        y_pos += y_step;
    }

    if (bounding_box) {
        BOX_X_Y_W_H (*bounding_box, 0, 0, x_step, UINT8_MAX);
    }
}

typedef struct {
    int origin;
    int key;
} int_key_t;

void int_key_print (int_key_t k)
{
    printf ("origin: %d, key: %d\n", k.origin, k.key);
}

templ_sort (sort_int_keys, int_key_t, a->key < b->key)

void increase_duplicated_coords (order_type_t *ot, int_key_t *key_array, int coord)
{
    int i;
    for (i=0; i<ot->n; i++) {
        key_array[i].key = ot->pts[i].E[coord];
        key_array[i].origin = i;
    }
    sort_int_keys (key_array, ot->n);

    int last_val = key_array[0].key;
    for (i=1; i<ot->n; i++) {
        if (last_val == key_array[i].key) {
            ot->pts[key_array[i].origin].E[coord]++;
        }
        last_val = key_array[i].key;
    }
}

void convex_ot_searchable (order_type_t *ot)
{
    convex_ot_scale (ot, 65535/2);

    int_key_t key_array[ot->n];
    increase_duplicated_coords (ot, key_array, VECT_X);
    increase_duplicated_coords (ot, key_array, VECT_Y);
}

void print_order_type (order_type_t *ot)
{
    int i = 0;
    if (ot->id != -1) {
        printf ("id: %"PRIu32"\n", ot->id);
    } else {
        printf ("id: unknown\n");
    }
    while (i < ot->n) {
        printf ("(%"PRIi64",%"PRIi64")\n", ot->pts[i].x, ot->pts[i].y);
        i++;
    }
    printf ("\n");
}

typedef struct {
    vect2i_t v [2];
} segment_t;

typedef struct {
    vect2i_t v [3];
} triangle_t;
#define TRIANGLE(ot,a,b,c) (triangle_t){{ot->pts[a],ot->pts[b],ot->pts[c]}}
#define TRIANGLE_IDXS(ot,idx) TRIANGLE(ot, idx[0], idx[1], idx[2])
#define TRIANGLE_IT(ot,it) TRIANGLE_IDXS(ot, it->idx)
void print_triangle (triangle_t *t)
{
    int i = 0;
    while (i < 3) {
        printf ("(%" PRIi64 ",%" PRIi64 ")\n", t->v[i].x, t->v[i].y);
        i++;
    }
    printf ("\n");
}

typedef struct {
    int k;
    triangle_t e [1];
} triangle_set_t;
#define triangle_set_size(n) (sizeof(triangle_set_t)+(n-1)*sizeof(triangle_t))

int64_t area_2 (vect2i_t a, vect2i_t b, vect2i_t c)
{
    return (b.x-a.x)*(c.y-a.y) - 
           (c.x-a.x)*(b.y-a.y);
}

int left (vect2i_t a, vect2i_t b, vect2i_t c)
{
    return area_2 (a, b, c) > 0;
}

int segments_intersect  (vect2i_t s1p1, vect2i_t s1p2, vect2i_t s2p1, vect2i_t s2p2)
{
    return
        (!left (s1p1, s1p2, s2p1) ^ !left (s1p1, s1p2, s2p2)) &&
        (!left (s2p1, s2p2, s1p1) ^ !left (s2p1, s2p2, s1p2));
}

typedef struct {
    int n;
    int size;
    int8_t triples [1];
} ot_triples_t;

ot_triples_t *ot_triples_new (order_type_t *ot, memory_stack_t *stack)
{
    ot_triples_t *ret;
    uint32_t size = ot->n*(ot->n-1)*(ot->n-2);
    if (stack) {
         ret = push_size (stack, sizeof(ot_triples_t)+(ot->n-1)*sizeof(int8_t));
    } else {
        ret = malloc(sizeof(ot_triples_t)+(ot->n-1)*sizeof(int8_t));
    }
    ret->n = ot->n;
    ret->size = size;

    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<ot->n; i++) {
        for (j=0; j<ot->n; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<ot->n; k++) {
                if (k==j || k==i) {
                    continue;
                }
                ret->triples[trip_id] = left(ot->pts[i], ot->pts[j], ot->pts[k]) ? 1 : -1;
                printf ("id:%d (%d, %d, %d) = %d\n", trip_id, i, j, k, ret->triples[trip_id]);
                trip_id ++;
            }
        }
    }
    printf ("\n");
    return ret;
}

// TODO: This is very slow.
void triple_from_id (int n, int id, int *a, int *b, int *c)
{
    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<n; i++) {
        for (j=0; j<n; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<n; k++) {
                if (k==j || k==i) {
                    continue;
                }
                if (trip_id == id) {
                    *a=i;
                    *b=j;
                    *c=k;
                }
                trip_id ++;
            }
        }
    }
}

void print_triple (int n, int id) {
    int a, b, c;
    triple_from_id (n, id, &a, &b, &c);
    printf ("id:%d (%d, %d, %d)\n", id, a, b, c);
}

int count_common_vertices (triangle_t *a, triangle_t *b)
{
    int i, j, res=0;
    for (i=0; i<3; i++) {
        for (j=0; j<3; j++) {
            // NOTE: The databese ensure no two points with the same X or Y
            // coordinate, so we can check only one.
            if (a->v[i].x == b->v[j].x) {
                res++;
            }
        }
    }
    return res;
}

int have_intersecting_segments (triangle_t *a, triangle_t *b)
{
    int i,j;
    for (i=0; i<3; i++) {
        for (j=0; j<3; j++) {
            if (segments_intersect (a->v[i], a->v[(i+1)%3], b->v[j], b->v[(j+1)%3])) {
                return 1;
            }
        }
    }
    return 0;
}

int is_thrackle (triangle_set_t *set)
{
    int i,j;
    for (i=0; i<set->k; i++) {
        for (j=i+1; j<set->k; j++) {
            int common_vertices = count_common_vertices (&set->e[i], &set->e[j]);
            if (common_vertices == 1) {
                continue;
            } else if (!have_intersecting_segments (&set->e[i], &set->e[j]) ||
                common_vertices == 2) {
                return 0;
            }
        }
    }
    return 1;
}

int thrackle_size (int n)
{
    switch (n) {
        case 3:
            return 1;
        case 4:
            return 1;
        case 5:
            return 2;
        case 6:
            return 4;
        case 7:
            return 7;
        case 8:
            return 8;
        case 9:
            return 10;
        case 10:
            return 12;
        default:
            printf ("Tn is unknown\n");
            return -1;
    }
}

int thrackle_size_lower_bound (int n)
{
    assert (n>=7);
    if (n%3 == 0) {
        return (n*n/9+1);
    } else {
        switch (n%6) {
            case 1:
                return (2*n*n-n+35)/18;
            case 4:
                return (2*n*n-n+26)/18;
            case 5:
                return (n*n-n+7)/9;
            case 2:
                return (n*n-n+16)/9;
            default:
                return -1;
        }
    }
}

int thrackle_size_upper_bound (int n)
{
    if (n%2 == 0) {
        return (n*n-2*n)/6;
    } else {
        return (n*n-n)/6;
    }
}

void thrackle_size_print (int n)
{
    int i=3;
    while (i<=n) {
        if (i<9) {
            printf ("T_%d: %d\n", i, thrackle_size (i));
        } else {
            printf ("T_%d: %d-%d\n", i, thrackle_size_lower_bound (i), thrackle_size_upper_bound(i));
        }
        i++;
    }
}

void init_example_thrackle (triangle_set_t *res, order_type_t *ot)
{
    if (res->k == 7) {
        res->e[0] = TRIANGLE(ot, 0, 1, 6);
        res->e[1] = TRIANGLE(ot, 0, 2, 3);
        res->e[2] = TRIANGLE(ot, 0, 4, 5);
        res->e[3] = TRIANGLE(ot, 6, 2, 5);
        res->e[4] = TRIANGLE(ot, 6, 3, 4);
        res->e[5] = TRIANGLE(ot, 1, 2, 4);
        res->e[6] = TRIANGLE(ot, 1, 3, 5);
    } else if (res->k == 8) {
        res->e[0] = TRIANGLE(ot, 0, 2, 7);
        res->e[1] = TRIANGLE(ot, 0, 3, 4);
        res->e[2] = TRIANGLE(ot, 0, 5, 6);
        res->e[3] = TRIANGLE(ot, 1, 2, 6);
        res->e[4] = TRIANGLE(ot, 1, 3, 5);
        res->e[5] = TRIANGLE(ot, 1, 4, 7);
        res->e[6] = TRIANGLE(ot, 2, 4, 5);
        res->e[7] = TRIANGLE(ot, 7, 3, 6);
    }
}

int is_edge_disjoint_set (triangle_set_t *set)
{
    int i,j;
    for (i=0; i<set->k; i++) {
        for (j=i+1; j<set->k; j++) {
            int common_vertices = count_common_vertices (&set->e[i], &set->e[j]);
            if (common_vertices == 2) {
                return 0;
            }
        }
    }
    return 1;
}

vect2_t v2_from_v2i (vect2i_t v)
{
    vect2_t res;
    res.x = (double)v.x;
    res.y = (double)v.y;
    return res;
}

vect2i_t v2i_from_v2 (vect2_t v)
{
    vect2i_t res;
    res.x = (int64_t)v.x;
    res.y = (int64_t)v.y;
    return res;
}

box_t box_min_dim (vect2_t min, vect2_t dim)
{
    box_t res;
    res.min = min;
    res.max = min;
    return res;
}

#define max_edge(box, res) max_min_edges(box,res, NULL)
#define min_edge(box, res) max_min_edges(box,NULL, res)
void max_min_edges (box_t box, int64_t *max, int64_t *min)
{
    int64_t width = box.max.x - box.min.x;
    int64_t height = box.max.y - box.min.y;
    int64_t l_max, l_min;
    if (width < height) {
        l_max = height;
        l_min = width;
    } else {
        l_min = height;
        l_max = width;
    }

    if (max) {
        *max = l_max;
    }
    if (min) {
        *min = l_min;
    }
}

void get_bounding_box (vect2_t *pts, int n, box_t *res)
{
    double min_x, min_y, max_x, max_y;

    min_x = max_x = pts[0].x;
    min_y = max_y = pts[0].y;

    int i=1;
    while (i < n) {
        if (pts[i].x < min_x) {
            min_x = pts[i].x;
        }

        if (pts[i].y < min_y) {
            min_y = pts[i].y;
        }

        if (pts[i].x > max_x) {
            max_x = pts[i].x;
        }

        if (pts[i].y > max_y) {
            max_y = pts[i].y;
        }
        i++;
    }
    res->min.x = min_x;
    res->min.y = min_y;
    res->max.x = max_x;
    res->max.y = max_y;
}

uint64_t factorial (int n)
{
    uint64_t res = 1;
    while (n>1) {
        res *= n;
        n--;
    }
    return res;
}

uint64_t binomial (int n, int k)
{
    uint64_t i, res = 1;
    for (i=0; i<k; i++) {
        res = (res*n)/(i+1);
        n--;
    }
    return res;
}

typedef struct {
    int n; // size of the whole set
    int k; // size of the subset
    uint64_t id; // absolute id of current indices
    uint64_t size; // total number of subsets
    int *idx; // actual indexes

    memory_stack_t *storage;

    int *precomp;
} subset_it_t;

void subset_it_reset_idx (int *idx, int k)
{
    while (0<k) {
        k--;
        idx[k] = k;
    }
}

void subset_it_init (subset_it_t *it, int n, int k, int *idx)
{
    it->id = 0;
    it->k = k;
    it->n = n;
    it->size = binomial (n, k);
    it->precomp = NULL;
    subset_it_reset_idx (idx, k);
}

subset_it_t *subset_it_new (int n, int k, memory_stack_t *stack)
{
    subset_it_t *retval;
    if (stack) {
        retval = push_struct (stack, subset_it_t);
        retval->idx = push_array (stack, k, int);
        retval->storage = stack;
    } else {
        retval = malloc (sizeof(subset_it_t));
        retval->idx = malloc (k*sizeof(int));
        retval->storage = NULL;
    }
    subset_it_init (retval, n, k, retval->idx);
    return retval;
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
// NOTE: This mutates the list idx and sorts it.
uint64_t subset_it_id_for_idx (int n, int *idx, int k)
{
    int_sort (idx, k);

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

void subset_it_idx_for_id (uint64_t id, int n, int *idx, int k)
{
    int i = 0;
    idx[0] = 0;

    while (i < k) {
        uint64_t subsets = binomial ((n)-(idx[i])-1, k-i-1);
        //printf ("bin(%d, %d)\n", (n)-(idx[k])-1, k-i-1);
        //printf ("k: %d, id: %ld, sus: %ld\n", k, id, subsets);
        if (id >= subsets) {
            id -= subsets;
            idx[i]++;
        } else {
            if (i<k-1) {
                idx[i+1] = idx[i]+1;
            }
            i++;
        }
    }
}

void subset_it_idx_for_id_safe (uint64_t id, int n, int *idx, int k)
{
    if (id >= binomial(n,k)) {
        return;
    }
    subset_it_idx_for_id (id, n, idx, k);
}

//// Code used to test subset_it_seek
//  subset_it_t *it = subset_it_new (8, 3, null);
//  do {
//      subset_it_print (it);
//  }while (subset_it_next (it));
//  printf ("\n");
//  {
//      int i=0;
//      for (i=0; i<it->size; i++) {
//          subset_it_seek (it, i);
//          subset_it_print (it);
//      }
//  }
//  subset_it_free (it);

void subset_it_seek (subset_it_t *it, uint64_t id)
{
    if (id >= it->size) {
        return;
    }

    it->id = id;
    if (it->precomp) {
        it->idx = &(it->precomp[id*it->k]);
    } else {
        subset_it_idx_for_id (id, it->n, it->idx, it->k);
    }
}

int subset_it_next_explicit (subset_it_t *it, int *idx)
{
    int j=it->k;
    it->id++;
    while (0<j) {
        j--;
        if (idx[j] < it->n-(it->k-j)) {
            idx[j]++;
            // reset indexes counting from changed one
            while (j<it->k-1) {
                idx[j+1] = idx[j]+1;
                j++;
            }
            return 1;
        }
    }
    assert (it->id == it->size);
    return 0;
}

int subset_it_next (subset_it_t *it) 
{
    if (it->id == it->size) {
        return 0;
    }

    if (it->precomp) {
        if (it->id < it->size) {
            it->id++;
            it->idx += it->k;
            return 1;
        } else {
            return 0;
        }
    } else {
        return subset_it_next_explicit (it, it->idx);
    }
}

// TODO: This is slow if not precomputed, try to implement a proper prev
// function in O(it->k).
void subset_it_prev (subset_it_t *it)
{
    if (it->id > 0) {
        it->id--;
        if (it->precomp) {
            it->idx -= it->k;
        } else {
            subset_it_seek (it, it->id);
        }
    } else {
        return;
    }
}

// NOTE: This function resets the iterator to id=0.
int subset_it_precompute (subset_it_t *it)
{
    if (it->precomp) {
        return 1;
    }

    int *precomp;

    if (it->storage) {
        precomp = push_array (it->storage, it->size*it->k, int);
    } else {
        precomp = malloc (sizeof(int)*it->size*it->k);
    }

    if (precomp) {
        subset_it_seek (it, 0);
        int *ptr = precomp;
        do {
            int k;
            for (k=0; k<it->k; k++) {
                *ptr = it->idx[k];
                ptr++;
            }
        } while (subset_it_next (it));

        if (!it->storage) {
            free (it->idx);
        }
        it->id = 0;
        it->idx = precomp;
        it->precomp = precomp;
        return 1;
    } else {
        return 0;
    }
}

void subset_it_print (subset_it_t *it)
{
    printf ("%lu: ", it->id);
    int i;
    for (i=0; i<it->k; i++) {
        printf ("%d ", it->idx[i]);
    }
    printf ("\n");
}

void subset_it_free (subset_it_t *it)
{
    assert (it->storage == NULL);
    if (it->precomp) {
        free (it->precomp);
    } else {
        free (it->idx);
    }
    free (it);
}

// Walker's backtracking algorithm used to generate only
// edge disjoint triangle sets.

// TODO: This backtracking procedure is not generic, maybe put backtrack
// specific data in some struct and receive a pointer to a function that
// updates this data.
bool backtrack (int *l, int *curr_seq, int domain_size, int *S_l,
        int *num_invalid, int *invalid_restore_indx, int *invalid_triangles)
{
    int lev = *l;
    lev--;
    if (lev>=0) {
        int i;
        for (i=curr_seq[lev]+1; i<domain_size; i++) {
            S_l[i] = 1;
        }

        *num_invalid = invalid_restore_indx[lev];
        for (i=0; i<*num_invalid; i++) {
            S_l[invalid_triangles[i]] = 0;
        }
        return true;
    } else {
        return false;
    }
}

void generate_edge_disjoint_triangle_sets (int n, int k, int *result, int *num_found)
{
    int l = 1; // Tree level
    int min_sl;
    bool S_l_empty;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_precompute (triangle_it);
    int total_triangles = triangle_it->size;

    int S_l[total_triangles];
    int i;
    for (i=1; i<triangle_it->size; i++) {
        S_l[i] = 1;
    }
    S_l[0] = 0;

    *num_found = 0;
    int set_id = 0;
    int triangle_id = 0;
    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int choosen_triangles[k];
    choosen_triangles[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    while (l > 0) {
        if (l>=k) {
            //array_print (choosen_triangles, k);
            int p_k;
            for (p_k=0; p_k<k; p_k++) {
                result[set_id+p_k] = choosen_triangles[p_k];
            }
            set_id += p_k;
            (*num_found)++;
            if (!backtrack (&l, choosen_triangles, total_triangles, S_l,
                       &num_invalid, invalid_restore_indx, invalid_triangles)) {
                break;
            }
        } else {
            // Compute S_l
            subset_it_seek (triangle_it, triangle_id);
            int triangle[3];
            triangle[0] = triangle_it->idx[0];
            triangle[1] = triangle_it->idx[1];
            triangle[2] = triangle_it->idx[2];

            int i;
            for (i=0; i<n; i++) {
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
                        S_l[id] = 0;
                        num_invalid++;
                    }
                }
            }
        }

        while (1) {
            // Try to advance
            S_l_empty = true;
            int i;
            for (i=0; i<total_triangles; i++) {
                if (S_l[i]) {
                    min_sl = i;
                    S_l_empty = false;
                    break;
                }
            }

            if (!S_l_empty) {
                triangle_id = min_sl;
                S_l[triangle_id] = 0;
                invalid_restore_indx[l] = num_invalid;
                choosen_triangles[l] = triangle_id;
                l++;
                break;
            }

            if (!backtrack (&l, choosen_triangles, total_triangles, S_l,
                       &num_invalid, invalid_restore_indx, invalid_triangles)) {
                break;
            }
        }
    }
}

// Walker's backtracking algorithm used to generate thrackles.
bool thrackle_backtrack (int *l, int *curr_seq, int domain_size, int *S_l,
        int *num_invalid, int *invalid_restore_indx, int *invalid_triangles)
{
    (*l)--;
    int lev = *l;
    if (lev>=0) {
        int i;
        for (i=curr_seq[lev]+1; i<domain_size; i++) {
            S_l[i] = 1;
        }

        *num_invalid = invalid_restore_indx[lev];
        for (i=0; i<*num_invalid; i++) {
            S_l[invalid_triangles[i]] = 0;
        }
        return true;
    } else {
        return false;
    }
}

typedef struct {
    float time;
    uint32_t num_found;
    uint32_t n;
    uint32_t k;
} search_info_t;

void file_write (int file, void *pos,  size_t size)
{
    if (write (file, pos, size) < size) {
        printf ("Write interrupted, aborting search\n");
    }
}

void iterate_threackles_backtracking (int n, int k, order_type_t *ot, char *filename)
{
    assert (n==ot->n);
    int l = 1; // Tree level
    int min_sl;
    bool S_l_empty;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_precompute (triangle_it);
    int total_triangles = triangle_it->size;

    int S_l[total_triangles];
    int i;
    for (i=1; i<triangle_it->size; i++) {
        S_l[i] = 1;
    }
    S_l[0] = 0;

    int triangle_id = 0;
    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int choosen_triangles[k];
    choosen_triangles[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    remove (filename);
    int file = open (filename, O_RDWR|O_CREAT, 0666);

    search_info_t info;
    lseek (file, sizeof (search_info_t), SEEK_SET);

    info.n = n;
    info.k = k;
    info.num_found = 0;
    struct timespec begin;
    struct timespec end;
    clock_gettime (CLOCK_MONOTONIC, &begin);

    while (l > 0) {
        if (l>=k) {
            file_write (file, choosen_triangles, k*sizeof(int));
            info.num_found++;
            if (info.num_found % 1000 == 0) {
                printf ("Found: %d\n", info.num_found);
            }
            if (!thrackle_backtrack (&l, choosen_triangles, total_triangles, S_l,
                       &num_invalid, invalid_restore_indx, invalid_triangles)) {
                continue;
            }
        } else {
            // Compute S_l
            subset_it_seek (triangle_it, triangle_id);
            int triangle[3];
            triangle[0] = triangle_it->idx[0];
            triangle[1] = triangle_it->idx[1];
            triangle[2] = triangle_it->idx[2];

            int i;
            for (i=0; i<n; i++) {
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
                        S_l[id] = 0;
                        num_invalid++;
                    }
                }
            }
        }

        while (1) {
            // Try to advance
            S_l_empty = true;
            int i;
            for (i=0; i<total_triangles; i++) {
                if (S_l[i]) {
                    min_sl = i;
                    S_l_empty = false;
                    break;
                }
            }

            if (!S_l_empty) {
                triangle_id = min_sl;

                subset_it_seek (triangle_it, triangle_id);
                triangle_t candidate_tr = TRIANGLE_IT (ot, triangle_it);

                bool is_disjoint = false;
                int j;
                for (j=0; j<l; j++) {
                    int choosen_id = choosen_triangles[j];
                    subset_it_seek (triangle_it, choosen_id);
                    triangle_t choosen_tr = TRIANGLE_IT (ot, triangle_it);
                    if (!have_intersecting_segments (&choosen_tr, &candidate_tr)) {
                        is_disjoint = true;
                        break;
                    }
                }

                S_l[triangle_id] = 0;
                if (!is_disjoint) {
                    invalid_restore_indx[l] = num_invalid;
                    choosen_triangles[l] = triangle_id;
                    l++;
                    break;
                } else {
                    continue;
                }
            }

            if (!thrackle_backtrack (&l, choosen_triangles, total_triangles, S_l,
                       &num_invalid, invalid_restore_indx, invalid_triangles)) {
                break;
            }
        }
    }

    clock_gettime (CLOCK_MONOTONIC, &end);
    info.time = time_elapsed_in_ms (&end, &begin);
    lseek (file, 0, SEEK_SET);
    file_write (file, &info, sizeof(search_info_t));
    close (file);
}

bool has_fixed_point (int n, int *perm_a, int *perm_b)
{
    int i;
    for (i=0; i<n; i++) {
        if (perm_a[i] == perm_b[i]) {
            return true;
        }
    }
    return false;
}

#define GET_PERM(i) &all_perms[(i)*n]
void print_decomp (int n, int* decomp, int* all_perms)
{
    int i;
    for (i=0; i<n; i++) {
        array_print (GET_PERM(decomp[i]), n);
    }
    printf ("\n");
}

// _res_ has to be of size sizeof(int)*n!
// NOTE: This function performs fast until n=10, here are some times:
// n:9 -> 0.332s
// n:10 -> 2.877s
// n:11 -> 37.355s
void compute_all_permutations (int n, int *res)
{
    int i;
    int *curr= res;
    for (i=1; i<=n; i++) {
        res[i-1] = i;
    }

    int found = 1;
    while (found < factorial(n)) {
        int k;
        for (k=0; k<n; k++) {
            curr[k+n] = curr[k];
        }
        curr+=n;

        int j;
        for (j=n-2; j>=0 && curr[j] >= curr[j+1]; j--) {
            if (j==0) {
                return;
            }
        }

        int l;
        for (l=n-1; curr[j]>=curr[l]; l--) {
            if (l < 0) {
                return;
            }
        }
        swap (&curr[j], &curr[l]);

        for (k=j+1, l=n-1; k<l;) {
            swap (&curr[k], &curr[l]);
            k++;
            l--;
        }
        found++;
    }
}

// TODO: Turns out this function is exactly the same as the one for other
// backtraching algorithms. I won't reuse it yet, because I want to spend more
// time designing more generic backtracking code that is easy to use (try to
// avoid function pointers).
void edges_from_permutation (int *all_perms, int perm, int n, int *e);
void print_2_regular_cycle_decomposition (int *edges, int n);

struct _backtrack_node_t {
    int id;
    int val;
    int num_children;
    struct _backtrack_node_t *children[1];
};
typedef struct _backtrack_node_t backtrack_node_t;

char* push_node (char* buff, int id, backtrack_node_t **children, int num_children)
{
    backtrack_node_t* node = (backtrack_node_t*)buff;
    node->id = id;
    node->num_children = num_children;
    int i;
    for (i=0; i<num_children; i++) {
        node->children[i] = children[i];
    }

    if (num_children > 1) {
        return buff+sizeof(backtrack_node_t)+(num_children-1)*sizeof(backtrack_node_t*);
    } else {
        return buff+sizeof(backtrack_node_t);
    }
}

// This algorithm returns a contiguous array of pointers to all resulting
// nodes with res[0] being the root of the tree.
//
// Memory Allocations:
// -------------------
//
// Everything will be allocated inside _pool_. Some data besides the resulting
// tree (Ex. _all_perms_ array) nedds to be stored, for this we use the local
// memory pool temp_pool. If this data is useful to the caller, it can instead
// be stored in _pool_ if a pointer is supplied as argument (_ret_all_perms_).

struct tree_build_st_t {
    uint32_t max_depth;
    uint32_t max_children;
    uint32_t max_node_size;
    uint32_t num_nodes_stack; // num_nodes_in_stack
    backtrack_node_t *node_stack;
    uint32_t num_nodes;
    cont_buff_t nodes;
    mem_pool_t temp_pool;
};

backtrack_node_t* stack_element (struct tree_build_st_t *tree_state, uint32_t i)
{
    return ((backtrack_node_t*)((char*)tree_state->node_stack + (i)*tree_state->max_node_size));
}

void push_partial_node (struct tree_build_st_t *tree_state, int val)
{
    backtrack_node_t *node = stack_element (tree_state, tree_state->num_nodes_stack);
    tree_state->num_nodes_stack++;

    *node = (backtrack_node_t){0};
    node->val = val;
    node->id = tree_state->num_nodes;
    tree_state->num_nodes++;
}

void complete_and_pop_node (struct tree_build_st_t *tree_state, mem_pool_t *pool, uint32_t l)
{
    backtrack_node_t *finished = stack_element (tree_state, l);
    uint32_t node_size;
    if (finished->num_children > 1) {
        node_size = sizeof(backtrack_node_t)+(finished->num_children-1)*sizeof(backtrack_node_t*);
    } else {
        node_size = sizeof(backtrack_node_t);
    }

    backtrack_node_t *pushed_node = mem_pool_push_size (pool, node_size);
    pushed_node->id = finished->id;
    pushed_node->val = finished->val;
    pushed_node->num_children = finished->num_children;
    int i;
    for (i=0; i<finished->num_children; i++) {
        pushed_node->children[i] = finished->children[i];
    }

    backtrack_node_t **node_pos = cont_buff_push (&tree_state->nodes, sizeof(backtrack_node_t*));
    *node_pos = pushed_node;

    tree_state->num_nodes_stack--;

    if (l > 0) {
        l--;
        backtrack_node_t *parent = stack_element (tree_state, l);
        parent->children[parent->num_children] = pushed_node;
        parent->num_children++;
    }
}

struct tree_build_st_t tree_start (uint32_t max_children, uint32_t max_depth)
{
    struct tree_build_st_t ret = {0};
    if (max_children > 0) {
        ret.max_children = max_children;
        ret.max_node_size =
            (sizeof(backtrack_node_t) +
             (ret.max_children-1)*sizeof(backtrack_node_t*));
    } else {
        // TODO: If we don't know max_children then use a version of
        // backtrack_node_t that has a cont_buff_t as children. Remember to
        // compute max_node_size in this case too.
        invalid_code_path;
    }

    if (max_depth > 0) {
        ret.max_depth = max_depth;
        ret.node_stack = mem_pool_push_size (&ret.temp_pool,(ret.max_depth+1)*ret.max_node_size);
    } else {
        // TODO: If we don't know max_depth then node_stack should be a
        // cont_buff_t of elements of size max_node_size.
        invalid_code_path;
    }
    // Pushing the root node to the stack.
    push_partial_node (&ret, -1);
    return ret;
}

backtrack_node_t* tree_end (struct tree_build_st_t *tree_state)
{
    backtrack_node_t* ret = ((backtrack_node_t**)tree_state->nodes.data)[tree_state->num_nodes-1];
    cont_buff_destroy (&tree_state->nodes);
    mem_pool_destroy (&tree_state->temp_pool);
    return ret;
}

bool matching_backtrack (int *l, int *curr_seq, int domain_size, bool *S_l,
                         int *num_invalid, int *invalid_restore_indx, int *invalid_perms,
                         struct tree_build_st_t *tree_state, mem_pool_t *res_pool)
{
    complete_and_pop_node (tree_state, res_pool, *l);
    (*l)--;
    if (*l >= 0) {
        int i;
        for (i=curr_seq[*l]+1; i<domain_size; i++) {
            S_l[i] = true;
        }

        *num_invalid = invalid_restore_indx[*l];
        for (i=0; i<*num_invalid; i++) {
            S_l[invalid_perms[i]] = false;
        }
        return true;
    } else {
        return false;
    }
}

backtrack_node_t* matching_decompositions_over_complete_bipartite_graphs
                                            (int n,
                                             mem_pool_t *pool, uint64_t *num_nodes,
                                             int *all_perms)
{
    assert (num_nodes != NULL);
    int num_perms = factorial (n);

    mem_pool_t temp_pool = {0};
    if (all_perms == NULL) {
        all_perms = mem_pool_push_array (&temp_pool, num_perms * n, int);
        compute_all_permutations (n, all_perms);
    }

    struct tree_build_st_t tree_state = tree_start (num_perms, n+1);

    int decomp[n];
    decomp[0] = 0;
    push_partial_node (&tree_state, 0);
    int l = 1;

    int invalid_restore_indx[n];
    invalid_restore_indx[0] = 0;
    int invalid_perms[num_perms];
    int num_invalid = 0;

    bool S_l[num_perms];
    S_l[0] = false;
    int i;
    for (i=1; i<num_perms; i++) {
        S_l[i] = true;
    }

    while (l>0) {
        if (l==n) {
            //int edges [4*n];
            //edges_from_permutation (all_perms, decomp[n-1], n, edges);
            //edges_from_permutation (all_perms, decomp[n-2], n, edges+2*n);
            //print_2_regular_cycle_decomposition (edges, n);

            //print_decomp (n, decomp, all_perms);
            //array_print (decomp, n);
            if (!matching_backtrack (&l, decomp, num_perms, S_l,
                                     &num_invalid, invalid_restore_indx, invalid_perms,
                                     &tree_state, pool)) {
                continue;
            }
        } else {
            // Compute S_l
            for (i=0; i<ARRAY_SIZE(S_l); i++) {
                if (S_l[i] && has_fixed_point (n, GET_PERM(decomp[l-1]), GET_PERM(i))) {
                    S_l[i] = false;
                    invalid_perms[num_invalid] = i;
                    num_invalid++;
                }
            }
        }

        while (1) {
            bool S_l_empty = true;
            int min_S_l = 0;
            for (i=0; i<num_perms; i++) {
                if (S_l[i]) {
                    min_S_l = i;
                    S_l_empty = false;
                    break;
                }
            }

            if (!S_l_empty) {
                invalid_restore_indx[l] = num_invalid;
                decomp[l] = min_S_l;
                S_l[min_S_l] = false;
                l++;

                push_partial_node (&tree_state, min_S_l);
                break;
            }

            if (!matching_backtrack (&l, decomp, num_perms, S_l,
                                     &num_invalid, invalid_restore_indx, invalid_perms,
                                     &tree_state, pool)) {
                break;
            }
        }
    }

    backtrack_node_t *root = tree_end (&tree_state);
    mem_pool_destroy (&temp_pool);

    return root;
}

// TODO: Currently we receive an array of all permutations of which _perm_ is an
// index, write an algorithm that computes it instead.
void edges_from_permutation (int *all_perms, int perm, int n, int *e)
{
    // NOTE: _e_ has to be of size 2*n
   int *permutation = &all_perms[perm*n];
   int i;
   for (i=0; i<n; i++) {
       e[2*i] = i;
       e[2*i+1] = permutation[i]+n-1;
   }
}

void edges_from_edge_subset (int *graph, int n, int *e)
{
    int i;
    for (i=0; i<2*n; i++) {
        e[2*i] = graph[i]/n;
        e[2*i+1] = graph[i]%n + n;
    }
}

// NOTE: num_edges = ARRAY_SIZE(edges)/2
void print_edges (int *edges, int num_edges)
{
    int i;
    for (i=0; i<num_edges; i++) {
        array_print (edges, 2);
        edges += 2;
    }
    printf ("\n");
}

typedef struct {
    int id;
    bool visited;
    int next_neighbor;
    int neighbors[2];
} order_2_node_t;

order_2_node_t* next_not_visited (order_2_node_t *trav_graph, int size)
{
    int i;
    for (i=0; i<size; i++) {
        order_2_node_t *node = &trav_graph[i];
        if (!node->visited) {
            return &trav_graph[i];
        }
    }
    return NULL;
}

void print_2_regular_cycle_decomposition (int *edges, int n)
{
    order_2_node_t trav_graph[2*n];
    memset (trav_graph, 0, sizeof(order_2_node_t)*ARRAY_SIZE(trav_graph));
    int i;
    for (i=0; i<ARRAY_SIZE(trav_graph); i++) {
        trav_graph[i].id = i;
    }

    int *e = edges;
    for (i=0; i<2*n; i++) {
        order_2_node_t *node = &trav_graph[e[0]];
        node->neighbors[node->next_neighbor] = e[1];
        node->next_neighbor++;

        node = &trav_graph[e[1]];
        node->neighbors[node->next_neighbor] = e[0];
        node->next_neighbor++;
        e += 2;
    }

    // NOTE: cycle_count[i] counts how many cycles of size 2*(i+2) are there.
    int cycle_count[n-1];
    array_clear (cycle_count, ARRAY_SIZE(cycle_count));
    order_2_node_t *curr;
    while ((curr = next_not_visited (trav_graph, ARRAY_SIZE(trav_graph)))) {
        int start = curr->id;
        int next_id = curr->neighbors[0];
        int cycle_size = 0;
        do {
            cycle_size++;
            int prev_id = curr->id;
            curr->visited = true;
            curr = &trav_graph[next_id];
            next_id = curr->neighbors[0] == prev_id? curr->neighbors[1] : curr->neighbors[0];
        } while (curr->id != start);
        cycle_count[cycle_size/2-2]++;
    }
    array_print (cycle_count, ARRAY_SIZE(cycle_count));
}


uint32_t count_2_regular_subgraphs_of_k_n_n (int n, int_dyn_arr_t *edges_out)
{
    // A subset of k_n_n edges of size 2*n
    int e_subs_2_n[2*n];

    int i;
    //for (i=0; i<n-1; i++) {
    //    printf ("%d ", 2*(i+2));
    //}
    //printf ("\n");
    //for (i=0; i<2*n-3; i++) {
    //    printf ("-");
    //}
    //printf ("\n");

    subset_it_t edge_subset_it;
    subset_it_init (&edge_subset_it, n*n, 2*n, e_subs_2_n);
    uint32_t count = 0;
    for (i=0; i<edge_subset_it.size; i++) {
        int node_orders[2*n];
        array_clear (node_orders, ARRAY_SIZE(node_orders));

        bool is_regular = true;
        int edges[4*n]; // 2vertices * 2partite * n
        edges_from_edge_subset (e_subs_2_n, n, edges);
        subset_it_next_explicit (&edge_subset_it, e_subs_2_n);

        int *e;
        for (e=edges; e < edges+ARRAY_SIZE(edges); e++) {
            node_orders[e[0]] += 1;
            if (node_orders[e[0]] > 2) {
                is_regular = false;
                break;
            }
        }

        if (is_regular) {
            //print_2_regular_graph (e_subs_2_n, n);
            //print_2_regular_cycle_decomposition (edges, n);
            if (edges_out) {
                int j;
                for (j=0; j<ARRAY_SIZE(edges); j++) {
                    int_dyn_arr_append (edges_out, edges[j]);
                }
            }
            count++;
        }
    }
    return count;
}
#define GEOMETRY_COMBINATORICS_H
#endif
