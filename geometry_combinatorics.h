#if !defined(GEOMETRY_COMBINATORICS_H)
#include <math.h>
#include "common.h"

// TODO: These are 128 bit structs, they may take up a lot of space, maybe have
// another one of 32 bits for small coordinates.
// TODO: Also, how does this affect performance?
typedef struct {
    int64_t x;
    int64_t y;
} vect2i_t;
#define VECT2i(x,y) ((vect2i_t){x,y})

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

void convex_ot (order_type_t *ot)
{
    assert (ot->n >= 3);
    double theta = (2*M_PI)/ot->n;
    ot->id = 0;
    int i;
    for (i=1; i<=ot->n; i++) {
        ot->pts[i-1].x = (int)(127.0*(1+cos(i*theta)));
        ot->pts[i-1].y = (int)(127.0*(1+sin(i*theta)));
    }
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

typedef union {
    struct {
        double x;
        double y;
    };
    double E[2];
} vect2_t;
#define VECT2(x,y) ((vect2_t){{x,y}})

double vect2_distance (vect2_t *v1, vect2_t *v2)
{
    return sqrt ((v1->x-v2->x)*(v1->x-v2->x) + (v1->y-v2->y)*(v1->y-v2->y));
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

typedef union {
    struct {
        double x;
        double y;
        double z;
    };

    struct {
        double r;
        double g;
        double b;
    };
    double E[3];
} vect3_t;
#define VECT3(x,y,z) ((vect3_t){{x,y,z}})

typedef struct {
    vect2_t min;
    vect2_t max;
} box_t;

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

void subset_it_reset_idx (subset_it_t *it)
{
    int k = it->k;
    while (0<k) {
        k--;
        it->idx[k] = k;
    }
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
    retval->id = 0;
    retval->k = k;
    retval->n = n;
    retval->size = binomial (n, k);
    retval->precomp = NULL;
    subset_it_reset_idx (retval);
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
    sort (idx, k);

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

int subset_it_next (subset_it_t *it) 
{
    if (it->id == it->size) {
        return 0;
    }

    it->id++;
    if (it->precomp) {
        if (it->id < it->size) {
            it->idx += it->k;
            return 1;
        } else {
            return 0;
        }
    } else {
        int j=it->k;
        while (0<j) {
            j--;
            if (it->idx[j] < it->n-(it->k-j)) {
                it->idx[j]++;
                // Reset indexes counting from changed one
                while (j<it->k-1) {
                    it->idx[j+1] = it->idx[j]+1;
                    j++;
                }
                return 1;
            }
        }
        assert (it->id == it->size);
        return 0;
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

void iterate_threackles_backtracking (int n, int k, order_type_t *ot, int *result, int *num_found)
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
            uint64_t choosen_set_id = subset_it_id_for_idx (total_triangles, choosen_triangles, k);
            printf ("%"PRIu64": ", choosen_set_id);
            array_print (choosen_triangles, k);
            //int p_k;
            //for (p_k=0; p_k<k; p_k++) {
            //    result[set_id+p_k] = choosen_triangles[p_k];
            //}
            //set_id += p_k;
            (*num_found)++;
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
}

// Automatic color palette:
//
// There are several way of doing this, using HSV with fixed SV and random Hue
// incremented every PHI_INV seems good enough. If it ever stops being good,
// research about perception based formats like CIELAB (LAB), and color
// difference functions like CIE76, CIE94, CIEDE2000.

#if 1
vect3_t color_palette[13] = {
    {{0.254902, 0.517647, 0.952941}}, //Google Blue
    {{0.858824, 0.266667, 0.215686}}, //Google Red
    {{0.956863, 0.705882, 0.000000}}, //Google Yellow
    {{0.058824, 0.615686, 0.345098}}, //Google Green
    {{0.666667, 0.274510, 0.733333}}, //Purple
    {{1.000000, 0.435294, 0.258824}}, //Deep Orange
    {{0.615686, 0.611765, 0.137255}}, //Lime
    {{0.937255, 0.380392, 0.568627}}, //Pink
    {{0.356863, 0.415686, 0.749020}}, //Indigo
    {{0.000000, 0.670588, 0.752941}}, //Teal
    {{0.756863, 0.090196, 0.352941}}, //Deep Pink
    {{0.619608, 0.619608, 0.619608}}, //Gray
    {{0.000000, 0.470588, 0.415686}}};//Deep Teal
#else

vect3_t color_palette[9] = {
    {{0.945098, 0.345098, 0.329412}}, // red
    {{0.364706, 0.647059, 0.854902}}, // blue
    {{0.980392, 0.643137, 0.227451}}, // orange
    {{0.376471, 0.741176, 0.407843}}, // green
    {{0.945098, 0.486275, 0.690196}}, // pink
    {{0.698039, 0.568627, 0.184314}}, // brown
    {{0.698039, 0.462745, 0.698039}}, // purple
    {{0.870588, 0.811765, 0.247059}}, // yellow
    {{0.301961, 0.301961, 0.301961}}};// gray
#endif

#define PHI_INV 0.618033988749895
#define COLOR_OFFSET 0.4

void get_next_color (vect3_t *color)
{
    static double h = COLOR_OFFSET;
    static int palette_idx = 0;

    // Reset color palette
    if (color == NULL) {
        palette_idx = 0;
        h = COLOR_OFFSET;
        return;
    }

    if (palette_idx < ARRAY_SIZE(color_palette)) {
        *color = color_palette[palette_idx];
        palette_idx++;
    } else {
        double s = 0.99;
        double v = 0.90;
        h += PHI_INV;
        if (h>1) {
            h -= 1;
        }

        int h_i = (int)(h*6);
        double frac = h*6 - h_i;

        double m = v * (1 - s);
        double desc = v * (1 - frac*s);
        double asc = v * (1 - (1 - frac) * s);

        switch (h_i) {
            case 0:
                color->r = v;
                color->g = asc;
                color->b = m;
                break;
            case 1:
                color->r = desc;
                color->g = v;
                color->b = m;
                break;
            case 2:
                color->r = m;
                color->g = v;
                color->b = asc;
                break;
            case 3:
                color->r = m;
                color->g = desc;
                color->b = v;
                break;
            case 4:
                color->r = asc;
                color->g = m;
                color->b = v;
                break;
            case 5:
                color->r = v;
                color->g = m;
                color->b = desc;
                break;
        }
    }
}

#define GEOMETRY_COMBINATORICS_H
#endif
