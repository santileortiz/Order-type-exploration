/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(GEOMETRY_COMBINATORICS_H)
#define GEOMETRY_COMBINATORICS_H
#include <math.h>
#include <time.h>
#include <string.h>
#include "sequence_store.h"

typedef struct {
    int n;
    vect2_t pts [1];
} real_point_set_t;

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
        default:
            // Maximum thrackle size is unknown
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
                invalid_code_path;
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

struct binomial_cache {
    void *b;
    uint32_t max_n;
    uint32_t max_k;
};
struct binomial_cache g_binomial_cache = {0};

void set_global_binomial_cache (uint32_t max_n, uint32_t max_k) {
    g_binomial_cache.max_n = max_n;
    g_binomial_cache.max_k = max_k;
    //TODO: We could be more inteligent about the size of the cache and instead
    //of using n*k cells only use ~n*k/2 by not allocating cells for when k>=n
    //and relying on binomial() to return 0 and 1 in these cases. This would
    //mean we would need macros to access g_binomial_cache.b and the current 2D
    //array idiom won't work.
    g_binomial_cache.b = malloc (sizeof(uint64_t)*max_n*max_k);
    memset (g_binomial_cache.b, 0, sizeof(uint64_t)*max_n*max_k);
}

void destroy_global_binomial_cache () {
    g_binomial_cache.max_n = 0;
    g_binomial_cache.max_k = 0;
    free (g_binomial_cache.b);
    g_binomial_cache.b = NULL;
}

uint64_t binomial (int n, int k)
{
    if (n < k) {
        return 0;
    } else if (n == k) {
        return 1;
    }

    bool store_in_cache = false;
    uint64_t (*b)[g_binomial_cache.max_n][g_binomial_cache.max_k];
    if (g_binomial_cache.b != NULL) {
        b = g_binomial_cache.b;
        if (n < g_binomial_cache.max_n && k < g_binomial_cache.max_k) {
            if ((*b)[n][k] != 0) {
                return (*b)[n][k];
            } else {
                store_in_cache = true;
            }
        }
    }

    uint64_t i, res = 1, n_loc = n;
    for (i=0; i<k; i++) {
        res = (res*n_loc)/(i+1);
        n_loc--;
    }

    if (store_in_cache) {
        (*b)[n][k] = res;
    }

    return res;
}

typedef struct {
    int n; // size of the whole set
    int k; // size of the subset
    uint64_t id; // absolute id of current indices
    uint64_t size; // total number of subsets
    int *idx; // actual indexes

    mem_pool_t *storage;

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

subset_it_t *subset_it_new (int n, int k, mem_pool_t *pool)
{
    subset_it_t *retval;
    if (pool) {
        retval = mem_pool_push_struct (pool, subset_it_t);
        retval->idx = mem_pool_push_array (pool, k, int);
        retval->storage = pool;
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

bool subset_it_next_explicit (int n, int k, int *idx)
{
    int j=k;
    while (0<j) {
        j--;
        if (idx[j] < n-(k-j)) {
            idx[j]++;
            // reset indexes counting from changed one
            while (j<k-1) {
                idx[j+1] = idx[j]+1;
                j++;
            }
            return true;
        }
    }
    return false;
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
        it->id++;
        return subset_it_next_explicit (it->n, it->k, it->idx);
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

// NOTE: res[] has to be of size binomial(n,k)*k
#define subset_it_computed_size(n,k) (binomial(n,k)*k*sizeof(int))
void subset_it_compute_all (int n, int k, int *res)
{
    int temp[k];
    subset_it_idx_for_id (0, n, temp, k);
    do{
        memcpy (res, temp, k*sizeof(int));
        res += k;
    } while (subset_it_next_explicit (n, k, temp));
}

int subset_it_precompute (subset_it_t *it)
{
    if (it->precomp) {
        return 1;
    }

    int *precomp;
    if (it->storage) {
        precomp = mem_pool_push_array (it->storage, it->size*it->k, int);
    } else {
        precomp = malloc (sizeof(int)*it->size*it->k);
    }

    if (precomp) {
        subset_it_compute_all (it->n, it->k, precomp);
        if (!it->storage) {
            free (it->idx);
        }
        it->idx = &precomp[it->id*it->k];
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

// Prints all integer partitions of _n_ in reverse lexicographic order.
// From: D. Knuth, The Art of Computer Programming Vol.4A p 392.
void print_all_partitions (int n)
{
    int part[n];
    int m = 1; // lenght of partition
    int i;
    for (i=1; i<n; i++) {
        part[i] = 1;
    }

    part[0] = n;
    int q = n>1 ? 0 : -1; // position of the last part >1.
    while (q >= 0) {
        array_print (part, m);
        if (part[q] == 2) {
            part[q] = 1;
            q--;
            m++;
        } else {
            int x = part[q]-1;
            part[q] = x;
            n = m-q;
            m = q+2;
            while (n > x) {
                part[m-1] = x;
                m++;
                n -= x;
            }
            part[m-1] = n;
            q = n==1 ? m-2 : m-1;
        }
    }
    array_print (part, m);
}

// SEQUENTIAL PARTITIONS
// The followoing functions split iteration over partitions. As an example, the
// following commented code reimplements print_all_partitions() with calls to them.
//
// void print_all_partitions (int n)
// {
//     int part[n];
//     int m, q;
//     init_partition (n, part, &m, &q);
//
//     do {
//         array_print (part, m);
//     } while (next_partition (n, part, &m, &q));
// }

// Initialize iteration over integer partitions of _n_.
// _part_ : Array of _n_ allocated elements.
// _m_ : Pointer to a signle integer. Size of resulting partition.
// _q_ : Pointer to a single integer. Highest index of a part >1.
void init_partition (int n, int *part, int *m, int *q)
{
    assert (m != NULL && q != NULL);
    *m = 1;
    int i;
    for (i=1; i<n; i++) {
        part[i] = 1;
    }
    *q = n>1 ? 0 : -1;
    part[0] = n;
}

// Updates _part_ to the next partition of _n_ in reverse lexicographic order,
// updating the number of partitions _m_, and the highest index _q_ of a non 1
// part.
// Returns: false if it's there are no more permutations, true otherwise.
// NOTE: _part_ must have _n_ elements allocated.
bool next_partition (int n, int *part, int *m, int *q)
{
    assert (m != NULL && q != NULL);
    if (*q < 0) {
        return false;
    }

    if (part[*q] == 2) {
        part[*q] = 1;
        (*q)--;
        (*m)++;
    } else {
        int x = part[*q]-1;
        part[*q] = x;
        n = *m-*q;
        *m = *q+2;
        while (n > x) {
            part[*m-1] = x;
            (*m)++;
            n -= x;
        }
        part[*m-1] = n;
        *q = n==1 ? *m-2 : *m-1;
    }

    return true;
}

// NUMBER OF (DOUBLY) RESTRICTED PARTITIONS
// Computes a 2D array into _p_ such that p[n][m] is the number of integer
// partitions of n with parts a_i s.t m >= a_i >= _k_. _N_ is the maximum value
// n (and m) can take (i.e. when _k_=1, p[n][n] is the partition number p(n)).
//
// To allocate _p_ptr_ as an array use:
//   int (*p_ptr)[N+1][N+1] = malloc (partition_restr_numbers_size(N));
//
// To access it then use:
//   (*p_ptr)[n][m];
//
// From: CACM Vol. 8, Issue 8, Algorithm 262, by J. K. S. McKay.
//       CACM Vol. 13, Issue 2, Algorithm 373, by John S. White.
//
// NOTE: if m>n, p[n][m] is undefined.
// NOTE: _p_ptr_ holds [N+1][N+1] integers.
#define partition_restr_numbers_size(N) ((N+1)*(N+1)*sizeof(int))

#define partition_restr_numbers(ptr,N) partition_dbl_restr_numbers(ptr,N,1)
void partition_dbl_restr_numbers (void *p_ptr, int N, int k)
{
    int (*p)[N+1][N+1] = (int (*)[N+1][N+1])p_ptr;
    if (k > 1) {
        memset (p, 0, partition_restr_numbers_size (N));
    }

    (*p)[0][0] = 1;
    int n;
    for (n=k; n<=N; n++) {
        (*p)[n][0] = 0;
        int m;
        for (m=k; m<=n; m++) {
            (*p)[n][m] = (*p)[n][m-1] + (*p)[n-m][n-m<m ? n-m: m];
        }
    }
}

void print_partition_sizes (void *p_ptr, int N)
{
    int (*p)[N+1][N+1] = (int (*)[N+1][N+1])p_ptr;
    int i;
    for (i=0; i<=N; i++) {
        array_print ((*p)[i], i+1);
    }
}

// This is SLOW for big _n_, allocates an array of (n+1)(n+1) ints, filling
// (n+1)(n+2)/2 cells in total. Which means it's O(n^2) in time and space.
// TODO: There are O(n) algorithms for this.
int partition_number (int n)
{
  int (*p)[n+1][n+1] = malloc (partition_restr_numbers_size(n));
  partition_restr_numbers (p, n);
  int res = (*p)[n][n];
  free (p);
  return res;
}

// RANDOM ACCESS (RESTRICTED) PARTITIONS
// The following functions allow random access to the list of integer partitions
// in reverse lexicographic order.

// Sets the integer partition _part_ and it's size _num_part_ corresponding to
// the index _id_ in the list of all partitions of _n_ in reverse lexicograpic
// order, with parts >=_k_. _p_ptr_ is the array computed
// by partition_dbl_restr_numbers().
//
// For unrestricted partitions (_k_=1) these macros are provided:
//  partition_restr_numbers(p_ptr,N)
//  partition_from_id(p_ptr,n,id,part,num_part)
//
// From: CACM Vol. 8, Issue 8, Algorithm 263, by J. K. S. McKay.
//       CACM Vol. 13, Issue 2, Algorithm 374, by John S. White.
// NOTE: _p_ptr_ must have been created with the same _k_ used in the call.
// NOTE: _part_ must have _n_ elements allocated.
// TODO: How to remove gotos?
#define partition_from_id(p_ptr,n,id,part,num_part) partition_restr_from_id(p_ptr,n,1,id,part,num_part)
void partition_restr_from_id (void *p_ptr, int n, int k, int id, int *part, int *num_part)
{
    int (*p)[n+1][n+1] = (int (*)[n+1][n+1])p_ptr;
    assert (id >= 0 && id < (*p)[n][n]);

    int n_loc = n;
    *num_part = 0;
    int psn = (*p)[n][n] - id - 1;

A:
    (*num_part)++;
    int m = k;
B:
    if ((*p)[n_loc][m] < psn) {
        m++;
        goto B;
    }

    if ((*p)[n_loc][m] > psn) {
C:
        part[*num_part-1] = m;
        psn -= (*p)[n_loc][m-1];
        n_loc -= m;
        if (n_loc < k) {
            return;
        } else {
            goto A;
        }
    } else {
        m++;
    }

    if (m == n_loc) {
        goto C;
    } else {
        goto B;
    }
}

// Returns the integer in [0,p(n)-1] that represents _part_ (parts in descending
// order) in the list of all partitions of _n_ in reverse lexicograpic order.
// _p_ptr_ is the array computed by partition_restr_numbers().
//
// From: CACM Vol. 8, Issue 8, Algorithm 264, by J. K. S. McKay.
// NOTE: For restricted partitions with parts >= k, create _p_ptr_ with that k.
int partition_to_id (void *p_ptr, int n, int *part)
{
    int n_loc = n;
    int (*p)[n+1][n+1] = (int (*)[n+1][n+1])p_ptr;
    int j=0, d = 0;
    while (n_loc != 0) {
        j++;
        d += (*p)[n_loc][part[j-1]-1];
        n_loc -= part[j-1];
    }
    return (*p)[n][n] - 1 - d;
}

// Prints all integer partitions of _n_ with parts grater than or equal to _k_.
void print_restricted_partitions (int n, int k)
{
    int (*p)[n+1][n+1] = malloc (partition_restr_numbers_size(n));
    partition_dbl_restr_numbers (p, n, k);
    int part[n], num_part;
    int i;
    for (i=0; i<(*p)[n][n]; i++) {
        partition_restr_from_id (p, n, k, i, part, &num_part);
        //Checks partitions to integers works fine too:
        //printf ("%d: ", partition_to_id (p, n, part));
        array_print (part, num_part);
    }
    free (p);
}

// Example code that shows how to use the random access functions. Also, serves
// as a test that algorithms are correctly implemented.
void partition_test_id (int n)
{
    int (*p)[n+1][n+1] = malloc (partition_restr_numbers_size(n));
    partition_restr_numbers (p, n);
    int part[n], num_part;

    int i;
    for (i=0; i<(*p)[n][n]; i++) {
        partition_from_id (p, n, i, part, &num_part);
        int res = partition_to_id (p, n, part);
        if (res != i) {
            printf ("There was an error on id %d.\n", i);
        }
    }
    free (p);
}

void triangle_set_from_ids (order_type_t *ot, int n, int *triangles, int k, triangle_set_t *set)
{
    int triangle[3];
    int i;
    set->k = k;
    for (i=0; i<k; i++) {
        subset_it_idx_for_id (triangles[i], ot->n, triangle, 3);
        set->e[i].v[0] = ot->pts[triangle[0]];
        set->e[i].v[1] = ot->pts[triangle[1]];
        set->e[i].v[2] = ot->pts[triangle[2]];
    }
}

// Walker's backtracking algorithm used to generate only
// edge disjoint triangle sets.

void generate_edge_disjoint_triangle_sets (int n, int k, struct sequence_store_t *seq)
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

    int triangle_id = 0;
    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int choosen_triangles[k];
    choosen_triangles[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    seq_set_length (seq, k, 0);
    seq_timing_begin (seq);

    while (l > 0) {
        if (l>=k) {
            seq_push_sequence (seq, choosen_triangles);
            if (seq_finish (seq)) {
                break;
            }
            goto backtrack;
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

backtrack:
            l--;
            if (l>=0) {
                int i;
                for (i=choosen_triangles[l]+1; i<total_triangles; i++) {
                    S_l[i] = 1;
                }

                num_invalid = invalid_restore_indx[l];
                for (i=0; i<num_invalid; i++) {
                    S_l[invalid_triangles[i]] = 0;
                }
            } else {
                break;
            }
        }
    }
    seq_timing_end (seq);
}

// NOTE: If ptr==NULL the result is undefined. DO NOT DO.
#define lb_idx(arr,ptr) (uint32_t)(ptr-arr)
struct linked_bool {
    struct linked_bool *next;
};

#define lb_print(ptr) {lb_print(ptr,ptr)}
void lb_print_start (struct linked_bool *lb, struct linked_bool *start)
{
    printf ("[");
    while (lb != NULL) {
        if (lb->next != NULL) {
            printf ("%d, ", lb_idx(start, lb));
        } else {
            printf ("%d]\n", lb_idx(start, lb));
        }
        lb = lb->next;
    }
}

// NOTE: res_triangles must be of size 3(n-3).
void triangles_with_common_edges (int n, int *triangle, int *res_triangles)
{
    int i, t_id = 0;
    for (i=0; i<n; i++) {
        int invalid_triangle[3];

        if (in_array(i, triangle, 3)) {
            continue;
        }

        int t;
        for (t=0; t<3; t++) {
            invalid_triangle[0] = i;
            invalid_triangle[1] = triangle[t];
            invalid_triangle[2] = triangle[(t+1)%3];
            res_triangles[t_id++] = subset_it_id_for_idx (n, invalid_triangle, 3);
        }
    }
}

// Receives arrays a and b of size 3
// Return values:
// 0 : arrays have no common elements
// 1 : arrays have one element in common
// 2 : arrays have 2 or more elements in common
int count_common_vertices_int (int *a, int *b)
{
    int i, j, res=0;
    for (i=0; i<3; i++) {
        for (j=0; j<3; j++) {
            if (a[i] == b[j]) {
                res++;
                if (res == 2) {
                    return res;
                }
            }
        }
    }

    return res;
}

#define LEX_TRIANG_ID(order,i) (order==NULL ? i : order[i])
int *get_all_triangles_array (int n, mem_pool_t *pool, int *triangle_order)
{
    int *res, *lex_triangles;
    int total_triangles = binomial (n,3);
    lex_triangles = mem_pool_push_size (pool, subset_it_computed_size(n,3));
    subset_it_compute_all (n, 3, lex_triangles);
    if (triangle_order == NULL) {
        res = lex_triangles;
    } else {
        res = mem_pool_push_size (pool, subset_it_computed_size(n,3));
        int *ptr_dest = res;
        while (ptr_dest < res+total_triangles*3) {
            memcpy (ptr_dest, lex_triangles+triangle_order[0]*3, 3*sizeof(int));
            triangle_order++;
            ptr_dest += 3;
        }
    }
    return res;
}

// This is for demonstration purposes only, shows what happens when we don't
// enforce the condition of sequences being in ascending order.
void thrackle_search_slow (int n, order_type_t *ot, struct sequence_store_t *seq,
                           int *triangle_order)
{
    assert (n==ot->n);
    int l = 1; // Tree level

    int total_triangles = binomial (n,3);
    struct linked_bool S[total_triangles];
    int i;
    for (i=0; i<total_triangles-1; i++) {
        S[i].next = &S[i+1];
    }
    S[i].next = NULL;

    struct linked_bool *t = &S[0];
    struct linked_bool *S_start = &S[0];

    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int k;
    if (n <= 9) {
        k = thrackle_size (n);
    } else {
        k = thrackle_size_upper_bound (n);
    }

    mem_pool_t temp_pool = {0};
    int *all_triangles = get_all_triangles_array (n, &temp_pool, triangle_order);

    seq_tree_extents (seq, total_triangles, k);
    seq_push_element (seq, LEX_TRIANG_ID(triangle_order,0), 0);

    int res[k];
    res[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    seq_timing_begin (seq);
    while (l > 0) {
        if (seq_finish (seq)) {
            break;
        }
        // Compute S
        if (t != NULL) {
            int *triangle = all_triangles + 3*lb_idx (S, t);
            triangle_t choosen_tr = TRIANGLE_IDXS (ot, triangle);

            struct linked_bool *S_prev = NULL;
            struct linked_bool *S_curr = S_start;
            while (S_curr != NULL) {
                int i = lb_idx (S, S_curr);
                int *candidate_tr_ids = all_triangles + 3*i;
                int test = count_common_vertices_int (triangle, candidate_tr_ids);
                if (test == 2) {
                    // NOTE: Triangles share an edge or are the same.
                    invalid_triangles[num_invalid++] = i;

                    if (S_prev == NULL) {
                        S_start = S_curr->next;
                    } else {
                        S_prev->next = S_curr->next;
                    }
                    S_curr = S_curr->next;
                    continue;
                } else if (test == 0) {
                    // NOTE: Triangles have no comon vertices, check if
                    // edges intersect.
                    triangle_t candidate_tr = TRIANGLE_IDXS (ot, candidate_tr_ids);
                    if (!have_intersecting_segments (&choosen_tr, &candidate_tr)) {
                        invalid_triangles[num_invalid++] = i;

                        if (S_prev == NULL) {
                            S_start = S_curr->next;
                        } else {
                            S_prev->next = S_curr->next;
                        }
                        S_curr = S_curr->next;
                        continue;
                    }
                }
                S_prev = S_curr;
                S_curr = S_curr->next;
            }
            t = S_start;
        }

        while (1) {
            // Try to advance
            if (t != NULL) {
                invalid_restore_indx[l] = num_invalid;
                res[l] = lb_idx(S, t);
                seq_push_element (seq, LEX_TRIANG_ID(triangle_order, res[l]), l);
                l++;
                break;
            }

            // Backtrack
            l--;
            if (l>=0) {
                t = &S[res[l]];
                struct linked_bool *S_prev = NULL;
                struct linked_bool *S_curr = S_start;
                int curr_invalid = invalid_restore_indx[l];
                while (curr_invalid<num_invalid) {
                    if (S_curr != NULL) {
                        while (lb_idx (S, S_curr)<invalid_triangles[curr_invalid]) {
                            S_prev = S_curr;
                            S_curr = S_curr->next;
                        }
                    }

                    while ((S_curr==NULL && curr_invalid<num_invalid) ||
                           (curr_invalid<num_invalid &&
                            lb_idx (S, S_curr)>invalid_triangles[curr_invalid])) {
                        if (S_prev == NULL) {
                            S_start = &S[invalid_triangles[curr_invalid]];
                            S_prev = S_start;
                            S_start->next = S_curr;
                        } else {
                            S_prev->next = &S[invalid_triangles[curr_invalid]];
                            S_prev->next->next = S_curr;
                            S_prev = S_prev->next;
                        }
                        curr_invalid++;
                    }

                    if (S_curr == NULL) {
                        break;
                    }
                }
                num_invalid = invalid_restore_indx[l];
                t = t->next;
            } else {
                break;
            }
        }
    }
    seq_timing_end (seq);
    mem_pool_destroy (&temp_pool);
}

bool single_thrackle (int n, int k, order_type_t *ot, int *res, int *count,
                           int *triangle_order)
{
    assert (n==ot->n);
    int l = 1; // Tree level

    int total_triangles = binomial (n,3);

    struct linked_bool S[total_triangles];
    int i;
    for (i=0; i<total_triangles-1; i++) {
        S[i].next = &S[i+1];
    }
    S[i].next = NULL;
    struct linked_bool *t = &S[0];

    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    mem_pool_t temp_pool = {0};
    int *all_triangles = get_all_triangles_array (n, &temp_pool, triangle_order);

    *count = 0;
    res[0] = 0;
    (*count)++;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    while (l > 0) {
        if (l>=k) {
            // Convert resulting thrackle to lexicographic ids of triangles, not
            // position in all_triangles array.
            for (i=0; i<k; i++) {
                res[i] = LEX_TRIANG_ID (triangle_order, res[i]);
            }

            if (triangle_order) {
                int_sort (res, k);
            }

            return true;
        } else {
            // Compute S
            int *triangle = all_triangles + 3*lb_idx (S, t);
            triangle_t choosen_tr = TRIANGLE_IDXS (ot, triangle);

            if (t != NULL) {
                // NOTE: S_curr=t->next enforces res[] to be an ordered sequence.
                struct linked_bool *S_prev = t;
                struct linked_bool *S_curr = t->next;
                while (S_curr != NULL) {
                    int i = lb_idx (S, S_curr);
                    int *candidate_tr_ids = all_triangles + 3*i;

                    int test = count_common_vertices_int (triangle, candidate_tr_ids);
                    if (test == 2) {
                        // NOTE: Triangles share an edge or are the same.
                        invalid_triangles[num_invalid++] = i;
                        S_prev->next = S_curr->next;

                        S_curr = S_prev->next;
                        continue;
                    } else if (test == 0) {
                        // NOTE: Triangles have no comon vertices, check if
                        // edges intersect.
                        triangle_t candidate_tr = TRIANGLE_IDXS (ot, candidate_tr_ids);
                        if (!have_intersecting_segments (&choosen_tr, &candidate_tr)) {
                            invalid_triangles[num_invalid++] = i;
                            S_prev->next = S_curr->next;

                            S_curr = S_prev->next;
                            continue;
                        }
                    }
                    S_prev = S_curr;
                    S_curr = S_curr->next;
                }
                t = t->next;
            }
        }

        while (1) {
            // Try to advance
            if (t != NULL) {
                invalid_restore_indx[l] = num_invalid;
                res[l] = lb_idx(S, t);
                l++;
                (*count)++;
                break;
            }

            // Backtrack
            l--;
            if (l>=0) {
                t = &S[res[l]];
                struct linked_bool *S_prev = NULL;
                struct linked_bool *S_curr = t;
                int curr_invalid = invalid_restore_indx[l];
                while (S_curr != NULL && curr_invalid<num_invalid) {
                    while (lb_idx (S, S_curr)<invalid_triangles[curr_invalid]) {
                        S_prev = S_curr;
                        S_curr = S_curr->next;
                    }

                    while ((S_curr==NULL && curr_invalid<num_invalid) ||
                           (curr_invalid<num_invalid &&
                            lb_idx (S, S_curr)>invalid_triangles[curr_invalid])) {
                        // NOTE: Should not happen because
                        // invalid_triangles[i]>lb_idx(t,S) for i>curr_invalid
                        // so previous loop must execute at least once.
                        assert (S_prev != NULL);
                        S_prev->next = &S[invalid_triangles[curr_invalid]];
                        S_prev->next->next = S_curr;

                        S_prev = S_prev->next;
                        curr_invalid++;
                    }
                }
                num_invalid = invalid_restore_indx[l];
                t = t->next;
            } else {
                break;
            }
        }
    }
    return false;
}

typedef struct {
    uint32_t n;
} th_file_info_t;

void all_thrackles (int n, int k, order_type_t *ot, struct sequence_store_t *seq)
{
    assert (n==ot->n);
    int l = 1; // Tree level

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_precompute (triangle_it);
    int total_triangles = triangle_it->size;

    struct linked_bool S[total_triangles];
    int i;
    for (i=0; i<total_triangles-1; i++) {
        S[i].next = &S[i+1];
    }
    S[i].next = NULL;
    struct linked_bool *t = &S[0];

    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int res[k];
    res[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    th_file_info_t info;
    info.n = n;

    seq_allocate_file_header (seq, sizeof(th_file_info_t));
    seq_set_length (seq, k, 0);
    seq_timing_begin (seq);

    while (l > 0) {
        if (l>=k) {
            seq_push_sequence (seq, res);
            goto backtrack;
        } else {
            // Compute S
            subset_it_seek (triangle_it, lb_idx (S, t));
            int triangle[3];
            triangle[0] = triangle_it->idx[0];
            triangle[1] = triangle_it->idx[1];
            triangle[2] = triangle_it->idx[2];

            triangle_t choosen_tr = TRIANGLE_IT (ot, triangle_it);

            if (t != NULL) {
                // NOTE: S_curr=t->next enforces res[] to be an ordered sequence.
                struct linked_bool *S_prev = t;
                struct linked_bool *S_curr = t->next;
                while (S_curr != NULL) {
                    int i = lb_idx (S, S_curr);
                    subset_it_seek (triangle_it, i);
                    int candidate_tr_ids[3];
                    candidate_tr_ids[0] = triangle_it->idx[0];
                    candidate_tr_ids[1] = triangle_it->idx[1];
                    candidate_tr_ids[2] = triangle_it->idx[2];

                    int test = count_common_vertices_int (triangle, candidate_tr_ids);
                    if (test == 2) {
                        // NOTE: Triangles share an edge or are the same.
                        invalid_triangles[num_invalid++] = i;
                        S_prev->next = S_curr->next;

                        S_curr = S_prev->next;
                        continue;
                    } else if (test == 0) {
                        // NOTE: Triangles have no comon vertices, check if
                        // edges intersect.
                        triangle_t candidate_tr = TRIANGLE_IT (ot, triangle_it);
                        if (!have_intersecting_segments (&choosen_tr, &candidate_tr)) {
                            invalid_triangles[num_invalid++] = i;
                            S_prev->next = S_curr->next;

                            S_curr = S_prev->next;
                            continue;
                        }
                    }
                    S_prev = S_curr;
                    S_curr = S_curr->next;
                }
                t = t->next;
            }
        }

        while (1) {
            // Try to advance
            if (t != NULL) {
                invalid_restore_indx[l] = num_invalid;
                res[l] = lb_idx(S, t);
                l++;
                break;
            }

backtrack:
            l--;
            if (l>=0) {
                t = &S[res[l]];
                struct linked_bool *S_prev = NULL;
                struct linked_bool *S_curr = t;
                int curr_invalid = invalid_restore_indx[l];
                while (S_curr != NULL && curr_invalid<num_invalid) {
                    while (lb_idx (S, S_curr)<invalid_triangles[curr_invalid]) {
                        S_prev = S_curr;
                        S_curr = S_curr->next;
                    }

                    while ((S_curr==NULL && curr_invalid<num_invalid) ||
                           (curr_invalid<num_invalid &&
                            lb_idx (S, S_curr)>invalid_triangles[curr_invalid])) {
                        // NOTE: Should not happen because
                        // invalid_triangles[i]>lb_idx(t,S) for i>curr_invalid
                        // so previous loop must execute at least once.
                        assert (S_prev != NULL);
                        S_prev->next = &S[invalid_triangles[curr_invalid]];
                        S_prev->next->next = S_curr;

                        S_prev = S_prev->next;
                        curr_invalid++;
                    }
                }
                num_invalid = invalid_restore_indx[l];
                t = t->next;
            } else {
                break;
            }
        }
    }
    seq_timing_end (seq);
    seq_write_file_header (seq, &info);
}

// TODO: Is this function really necessary? Finish implementing sequence_store_t
// so seq_push_sequence() builds a tree too, then test if there is actually a
// signifficant difference between this and all_thrackles().
#define thrackle_search_tree(n,ot,seq) thrackle_search_tree_full(n,ot,seq,NULL)
void thrackle_search_tree_full (int n, order_type_t *ot, struct sequence_store_t *seq,
                                int *triangle_order)
{
    assert (n==ot->n);
    int l = 1; // Tree level

    int total_triangles = binomial (n,3);
    struct linked_bool S[total_triangles];
    int i;
    for (i=0; i<total_triangles-1; i++) {
        S[i].next = &S[i+1];
    }
    S[i].next = NULL;
    struct linked_bool *t = &S[0];

    int invalid_triangles[total_triangles];
    int num_invalid = 0;

    int k;
    if (n <= 9) {
        k = thrackle_size (n);
    } else {
        k = thrackle_size_upper_bound (n);
    }

    mem_pool_t temp_pool = {0};
    int *all_triangles = get_all_triangles_array (n, &temp_pool, triangle_order);

    seq_tree_extents (seq, total_triangles, k);
    seq_push_element (seq, LEX_TRIANG_ID(triangle_order,0), 0);

    int res[k];
    res[0] = 0;
    int invalid_restore_indx[k];
    invalid_restore_indx[0] = 0;

    seq_timing_begin (seq);
    while (l > 0) {
        if (seq_finish (seq)) {
            break;
        }
        // Compute S
        if (t != NULL) {
            int *triangle = all_triangles + 3*lb_idx(S, t);
            triangle_t choosen_tr = TRIANGLE_IDXS (ot, triangle);

            // NOTE: S_curr=t->next enforces res[] to be an ordered sequence.
            struct linked_bool *S_prev = t;
            struct linked_bool *S_curr = t->next;
            while (S_curr != NULL) {
                int i = lb_idx (S, S_curr);
                int *candidate_tr_ids = all_triangles + i*3;
                int test = count_common_vertices_int (triangle, candidate_tr_ids);
                if (test == 2) {
                    // NOTE: Triangles share an edge or are the same.
                    invalid_triangles[num_invalid++] = i;
                    S_prev->next = S_curr->next;

                    S_curr = S_prev->next;
                    continue;
                } else if (test == 0) {
                    // NOTE: Triangles have no comon vertices, check if
                    // edges intersect.
                    triangle_t candidate_tr = TRIANGLE_IDXS (ot, candidate_tr_ids);
                    if (!have_intersecting_segments (&choosen_tr, &candidate_tr)) {
                        invalid_triangles[num_invalid++] = i;
                        S_prev->next = S_curr->next;

                        S_curr = S_prev->next;
                        continue;
                    }
                }
                S_prev = S_curr;
                S_curr = S_curr->next;
            }
            t = t->next;
        }

        while (1) {
            // Try to advance
            if (t != NULL) {
                invalid_restore_indx[l] = num_invalid;
                res[l] = lb_idx(S, t);
                seq_push_element (seq, LEX_TRIANG_ID(triangle_order, res[l]), l);
                l++;
                break;
            }

            // Backtrack
            l--;
            if (l>=0) {
                t = &S[res[l]];
                struct linked_bool *S_prev = NULL;
                struct linked_bool *S_curr = t;
                int curr_invalid = invalid_restore_indx[l];
                while (S_curr != NULL && curr_invalid<num_invalid) {
                    while (lb_idx (S, S_curr)<invalid_triangles[curr_invalid]) {
                        S_prev = S_curr;
                        S_curr = S_curr->next;
                    }

                    while ((S_curr==NULL && curr_invalid<num_invalid) ||
                           (curr_invalid<num_invalid &&
                            lb_idx (S, S_curr)>invalid_triangles[curr_invalid])) {
                        // NOTE: Should not happen because
                        // invalid_triangles[i]>lb_idx(t,S) for i>curr_invalid
                        // so previous loop must execute at least once.
                        assert (S_prev != NULL);
                        S_prev->next = &S[invalid_triangles[curr_invalid]];
                        S_prev->next->next = S_curr;

                        S_prev = S_prev->next;
                        curr_invalid++;
                    }
                }
                num_invalid = invalid_restore_indx[l];
                t = t->next;
            } else {
                break;
            }
        }
    }
    seq_timing_end (seq);
    mem_pool_destroy (&temp_pool);
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
void K_n_n_1_factorizations (int n, int *all_perms, struct sequence_store_t *seq)
{
    assert (seq != NULL);
    int num_perms = factorial (n);

    mem_pool_t temp_pool = {0};
    if (all_perms == NULL) {
        all_perms = mem_pool_push_array (&temp_pool, num_perms * n, int);
        compute_all_permutations (n, all_perms);
    }

    seq_tree_extents (seq, num_perms, n);

    int decomp[n];
    decomp[0] = 0;
    seq_push_element (seq, 0, 0);
    int l = 1;

    int invalid_restore_indx[n];
    invalid_restore_indx[0] = 0;
    int invalid_perms[num_perms];
    int num_invalid = 0;

    struct linked_bool *S_l = mem_pool_push_size (&temp_pool, num_perms*sizeof(struct linked_bool));
    struct linked_bool *first_S_l[n];
    struct linked_bool *bak_first_S_l[n];
    {
        int i, j=1;
        for (i=0; i<num_perms-1; i++) {
            if ((i+1)%(num_perms/n) == 0) {
                S_l[i].next = NULL;
                first_S_l[j++] = &S_l[i+1];
            } else {
                S_l[i].next = &S_l[i+1];
            }
        }
        S_l[num_perms-1].next = NULL;
        first_S_l[0] = S_l + 1;
    }

    seq_timing_begin (seq);
    while (l>0) {
        if (l==n) {
            goto backtrack;
        } else {
            // Compute S_l
            uint32_t h;
            for (h=l; h<n; h++) {
                struct linked_bool *prev_S_l = NULL;
                struct linked_bool *iter_S_l = first_S_l[h];
                while (iter_S_l != NULL) {
                    uint32_t i = lb_idx (S_l, iter_S_l);
                    if (has_fixed_point (n, GET_PERM(decomp[l-1]), GET_PERM(i))) {
                        invalid_perms[num_invalid] = i;
                        num_invalid++;

                        if (prev_S_l == NULL) {
                            first_S_l[h] = iter_S_l->next;
                        } else {
                            prev_S_l->next = iter_S_l->next;
                        }
                    } else {
                        // NOTE: If we removed an element, then prev_S_l stays
                        // the same.
                        prev_S_l = iter_S_l;
                    }
                    iter_S_l = iter_S_l->next;
                }
            }
            bak_first_S_l[l] = first_S_l[l];
        }

        while (1) {
            if (first_S_l[l] != NULL) {
                int min_S_l = lb_idx (S_l, first_S_l[l]);
                invalid_restore_indx[l] = num_invalid;
                decomp[l] = min_S_l;
                first_S_l[l] = first_S_l[l]->next;
                seq_push_element (seq, min_S_l, l);
                l++;
                break;
            }


backtrack:
            if (l < n) {
                first_S_l[l] = bak_first_S_l[l];
            }

            (l)--;
            if (l >= 0) {
                uint32_t prev_invalid = num_invalid;
                num_invalid = invalid_restore_indx[l];
                uint32_t curr_restore = num_invalid;

                uint32_t h;
                for (h=l+1; h<n; h++) {
                    struct linked_bool *S_l_prev = NULL;
                    struct linked_bool *S_l_iter = first_S_l[h];
                    uint32_t max_for_h = (h+1)*num_perms/n;
                    while (S_l_iter != NULL &&
                           curr_restore < prev_invalid && invalid_perms [curr_restore] < max_for_h) {

                        while (S_l_iter != NULL &&
                               lb_idx (S_l, S_l_iter) < invalid_perms [curr_restore]) {
                            S_l_prev = S_l_iter;
                            S_l_iter = S_l_iter->next;
                        }

                        while (curr_restore < prev_invalid &&
                               (S_l_iter == NULL || lb_idx(S_l, S_l_iter) > invalid_perms[curr_restore]) &&
                               invalid_perms[curr_restore] < max_for_h) {

                            if (S_l_prev != NULL) {
                                S_l_prev->next = &S_l[invalid_perms [curr_restore]];
                                S_l_prev->next->next = S_l_iter;
                                S_l_prev = S_l_prev->next;
                            } else {
                                first_S_l[h] = &S_l[invalid_perms [curr_restore]];
                                S_l_prev = first_S_l[h];
                                S_l_prev->next = S_l_iter;
                            }
                            curr_restore++;
                        }
                    }
                }

            } else {
                break;
            }
        }
    }
    seq_timing_end (seq);
    mem_pool_destroy (&temp_pool);
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


// Interprets _edge_subset_ as a list of labels where each one represents an edge
// of K_n_n. Edges of K_n_n are labeled as if vertices where labeled from 0 to
// n-1 on each side, then each vertex id on the left increments the edge id by n
// while the id of the right side increments it by 1.
//
//      0  ●   ●  (0)  n
//      1  ●   ●  (1)  n+1 (b)
//         ●  /●
//           / ---------> (n-2)*n + 1 = (n-2)*n + (n+1)%n = a*n + b%n
//          /
// (a) n-2 ●   ● (n-2) 2n-1
//     n-1 ●   ● (n-1) 2n
//
void edges_from_edge_subset (int *edge_subset, int n, int *e)
{
    int i;
    for (i=0; i<2*n; i++) {
        e[2*i] = edge_subset[i]/n;
        e[2*i+1] = edge_subset[i]%n + n;
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

// Sets the array _cycle_count_ such that cycle_count[i] counts how many cycles
// of size 2*(i+2) are there. _edges_ and _n_ represent a complete bipartite
// graph K_n_n.
// NOTE: _cycle_count_ must have n-1 elements.
void cycle_count_from_2_regular (int *edges, int n, int *cycle_count)
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

    array_clear (cycle_count, n-1);
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
}

// Returns the integer partition of _n_ that represents half of the sizes of the
// cycles of the 2-regular graph represented by _edges_. Where _edges_ is a list
// of integers where each consecutive pair contains indexes of the vertices of
// an edge.
// NOTE: _edges_ is assumed to be 2-regular.
// NOTE: cycles on _edges_ are assumed to be even. Currently we only use this on
// a subgraphs of K_n_n so it's true that cycles are always even.
void partition_from_2_regular (int *edges, int n, int *part, int *num_part)
{
    int cycle_count[n-1];
    cycle_count_from_2_regular (edges, n, cycle_count);
    *num_part = 0;
    array_clear (part, n/2+1); // Longest partition is {2,...,2,3}
    int i = n-2;
    while (i >= 0) {
        int j;
        for (j=0; j < cycle_count[i]; j++) {
            part[*num_part] = i+2;
            (*num_part)++;
        }
        i--;
    }
}

void print_2_regular_cycle_count (int *edges, int n)
{
    int cycle_count[n-1];
    cycle_count_from_2_regular (edges, n, cycle_count);
    array_print (cycle_count, ARRAY_SIZE(cycle_count));
}

uint64_t e_n (int n) {
    uint64_t res = 1;
    int i;
    for (i=1; i<=n; i++) {
        res *= i;
    }
    return res*res/(2*n);
}

// Implements the derived formula that counts the number of 2-factors of the
// complete bipartite graph K_n_n, with the same characteristic multiset _A_.
// n is computed from the sum of elements in _A_.
// NOTE: array _A_ must be sorted.
uint64_t cnt_2_factors_of_k_n_n_for_A (int *A, int A_len)
{
    int i, n=0;
    {
        bool ascending = true, descending = true;
        for (i=0; i<A_len; i++) {
            if (A[i] == 1) {
                printf ("A can't contain 1.\n");
                return 0;
            }

            if (i < A_len-1) {
                if (A[i] > A[i+1]) ascending = false;
                if (A[i] < A[i+1]) descending = false;
            }
            n += A[i];
        }
        if (!ascending && !descending) {
            printf ("A is not sorted\n");
            return 0;
        }
    }

    int S_x = 0;
    uint64_t res = 1;
    i=0;
    while (i < A_len) {
        int k = 0;
        uint64_t tmp_repet = 1;
        do {
            uint64_t tmp = binomial(n-S_x, A[i]);
            tmp_repet *= tmp*tmp;

            S_x += A[i];
            k++;
            i++;
            if (i == A_len) break;
        } while (A[i-1] == A[i]);

        uint64_t e = e_n(A[i-1]);
        uint64_t e_n_pw_k = 1;
        uint64_t k_fact = 1;
        int j;
        for (j=0; j<k; j++) {
            e_n_pw_k *= e;
            k_fact *= k-j;
        }
        tmp_repet = e_n_pw_k*(tmp_repet/k_fact);
        res *= tmp_repet;
    }
    return res;
}

// Returns true if the graph G of _num_vert_ vertices represented by _edges_ is
// 2-regular. The _edges_ array has 2*E(G) (2*_num_edges) elements, where each
// consecutive pair contains indexes of the vertices of one edge.
// NOTE: Assumes there are no repeated edges.
bool is_2_regular (int num_vert, int *edges, int num_edges)
{
    int node_orders[num_vert];
    array_clear (node_orders, ARRAY_SIZE(node_orders));

    int *e = edges, verts_done = 0;
    while (verts_done < 2*num_edges) {
        verts_done++;
        node_orders[e[0]] += 1;
        if (node_orders[e[0]] > 2) {
            return false;
        }
        e++;
    }
    return true;
}

// Counts the number of subgraphs of K_n_n with 2n edges that are 2-regular
// (i.e. the 2-factors of K_n_n). It uses K_n_n with vetices tagged as follows:
//
//   0  ●   ● n
//   1  ●   ● n+1
//   ·        ·
//   ·        ·
//   ·        ·
//  n-2 ●   ● 2n-1
//  n-1 ●   ● 2n
//
//  If _edges_out_ != NULL then we store all edges of each 2-regular subgraph in
//  it. It will be a sequence of sets of 4n integers where each consecutive pair
//  represent vertex ids for one edge.
//
uint32_t count_2_regular_subgraphs_of_k_n_n (int n, int_dyn_arr_t *edges_out)
{
    // A subset of k_n_n edges of size 2*n
    int e_subs_2_n[2*n];
    subset_it_reset_idx (e_subs_2_n, 2*n);

    uint32_t count = 0;
    do {
        int edges[4*n]; // 2vertices * 2partite * n
        edges_from_edge_subset (e_subs_2_n, n, edges);

        // NOTE: Because we have 2*n distinct edges in the graph (by
        // construction), checking regularity implies it's also spanning.
        if (is_2_regular (2*n, edges, 2*n)) {
            if (edges_out) {
                int j;
                for (j=0; j<ARRAY_SIZE(edges); j++) {
                    int_dyn_arr_append (edges_out, edges[j]);
                }
            } else {
                //print_2_regular_graph (e_subs_2_n, n);
                print_2_regular_cycle_count (edges, n);
            }

            count++;
        }
    } while (subset_it_next_explicit (n*n, 2*n, e_subs_2_n));
    return count;
}

void edge_sizes (int n, int *t, int *dist)
{
    int d_a = 0;
    int d_b = 0;
    d_a = t[1] - t[0] - 1;
    d_b = n + t[0] - t[1] - 1;
    dist[0] = MIN (d_a, d_b);

    d_a = t[2] - t[1] - 1;
    d_b = n + t[1] - t[2] - 1;
    dist[1] = MIN (d_a, d_b);

    d_a = t[2] - t[0] - 1;
    d_b = n + t[0] - t[2] - 1;
    dist[2] = MIN (d_a, d_b);
    int_sort (dist, 3);
}

int triangle_size (int n, int *t)
{
    int dist[3];
    edge_sizes (n, t, dist);

    int h;
    int res = 0;
    for (h=0; h<3; h++) {
        if (dist[h] > res) {
            res = dist[h];
        }
    }
    return res;
}

int tr_less_than (int n, int a, int b)
{
    int dist_a[3];
    int t_a[3];
    subset_it_idx_for_id (a, n, t_a, 3);
    edge_sizes (n, t_a, dist_a);

    int dist_b[3];
    int t_b[3];
    subset_it_idx_for_id (b, n, t_b, 3);
    edge_sizes (n, t_b, dist_b);

    if (dist_a[0] < dist_b[0]) {
        return 1;
    } else if (dist_a[1] < dist_b[1]) {
        return 1;
    } else if (dist_a[2] < dist_b[2]) {
        return 1;
    } else {
        return 0;
    }
}

templ_sort (lex_size_triangle_sort, int, tr_less_than (*(int*)user_data, *a, *b))

void order_triangles_size (int n, int *triangle_order)
{
    int total_triangles = binomial (n,3);
    int_key_t *sort_structs = malloc (sizeof(int_key_t)*(total_triangles));
    int i;
    for (i=0; i<total_triangles; i++) {
        sort_structs[i].origin = i;

        int t[3];
        subset_it_idx_for_id (i, n, t, 3);
        sort_structs[i].key = triangle_size (n, t);
    }
    sort_int_keys (sort_structs, total_triangles);

    for (i=0; i<total_triangles; i++) {
        triangle_order[i] = sort_structs[i].origin;
    }
    free (sort_structs);
}
#endif
