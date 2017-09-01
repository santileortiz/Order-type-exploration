/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(COMMON_H)
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
typedef enum {false, true} bool;

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#if !defined(MIN)
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define kilobyte(val) ((val)*1024LL)
#define megabyte(val) (kilobyte(val)*1024LL)
#define gigabyte(val) (megabyte(val)*1024LL)
#define terabyte(val) (gigabyte(val)*1024LL)

#define invalid_code_path assert(0)

#define VECT_X 0
#define VECT_Y 1
#define VECT_Z 2
#define VECT_W 3

#define VECT_R 0
#define VECT_G 1
#define VECT_B 2
#define VECT_A 3

typedef union {
    struct {
        double x;
        double y;
    };
    double E[2];
} vect2_t;
#define VECT2(x,y) ((vect2_t){{x,y}})

typedef struct {
    vect2_t min;
    vect2_t max;
} box_t;

#define BOX_X_Y_W_H(box,n_x,n_y,n_w,n_h) {(box).min.x=(n_x);(box).max.x=(n_x)+(n_w); \
                                          (box).min.y=(n_y);(box).max.y=(n_y)+(n_h);}
#define BOX_CENTER_X_Y_W_H(box,n_x,n_y,n_w,n_h) {(box).min.x=(n_x)-(n_w)/2;(box).max.x=(n_x)+(n_w)/2; \
                                                 (box).min.y=(n_y)-(n_h)/2;(box).max.y=(n_y)+(n_h)/2;}
#define BOX_POS_SIZE(box,pos,size) BOX_X_Y_W_H(box,(pos).x,(pos).y,(size).x,(size).y)

#define BOX_WIDTH(box) ((box).max.x-(box).min.x)
#define BOX_HEIGHT(box) ((box).max.y-(box).min.y)
#define BOX_AR(box) (BOX_WIDTH(box)/BOX_HEIGHT(box))

double vect2_distance (vect2_t *v1, vect2_t *v2)
{
    return sqrt ((v1->x-v2->x)*(v1->x-v2->x) + (v1->y-v2->y)*(v1->y-v2->y));
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

typedef union {
    struct {
        double x;
        double y;
        double z;
        double w;
    };

    struct {
        double r;
        double g;
        double b;
        double a;
    };

    struct {
        double h;
        double s;
        double l;
    };
    double E[4];
} vect4_t;
#define VECT4(x,y,z,w) ((vect4_t){{x,y,z,w}})

void vect4_print (vect4_t *v)
{
    printf ("(%f, %f, %f, %f)", v->x, v->y, v->z, v->w);
}

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t used;
} memory_stack_t;

void memory_stack_init (memory_stack_t *stack, uint32_t size, uint8_t* data)
{
    stack->data = data;
    stack->size = size;
    stack->used = 0;
}

#define push_struct(memory_stack, type) push_size(memory_stack, sizeof(type))
#define push_array(memory_stack, n, type) push_size(memory_stack, n*sizeof(type))
void *push_size (memory_stack_t *stack, uint32_t size)
{
    assert (stack->used+size <= stack->size);
    void *res = &stack->data[stack->used];
    stack->used += size;
    return res;
}

typedef struct {
    memory_stack_t *stack;
    uint32_t used;
} temporary_marker_t;

temporary_marker_t begin_temporary_memory (memory_stack_t *stack)
{
    temporary_marker_t res;
    res.used = stack->used;
    res.stack = stack;
    return res;
}

void end_temporary_memory (temporary_marker_t mrkr)
{
    assert (mrkr.stack->used >= mrkr.used);
    mrkr.stack->used = mrkr.used;
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

void array_clear (int *arr, int n)
{
    while (n) {
        n--;
        arr[n] = 0;
    }
}

void array_print (int *arr, int n)
{
    int i;
    for (i=0; i<n-1; i++) {
        printf ("%d ", arr[i]);
    }
    printf ("%d\n", arr[i]);
}

void print_uint_array (uint32_t *arr, int n)
{
    printf ("[");
    int i;
    for (i=0; i<n-1; i++) {
        printf ("%"PRIu32", ", arr[i]);
    }
    printf ("%"PRIu32"]\n", arr[i]);
}

void swap (int*a, int*b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Merge sort implementation
void int_sort (int *arr, int n)
{
    if (n==1) {
        return;
    } else if (n == 2) {
        if (arr[1] < arr[0]) {
            swap (&arr[0], &arr[1]);
        }
    } else if (n==3) {
        if (arr[0] > arr[1]) swap(&arr[0],&arr[1]);
        if (arr[1] > arr[2]) swap(&arr[1],&arr[2]);
        if (arr[0] > arr[1]) swap(&arr[0],&arr[1]);
    } else {
        int res[n];
        int_sort (arr, n/2);
        int_sort (&arr[n/2], n-n/2);

        int i;
        int a=0;
        int b=n/2;
        for (i=0; i<n; i++) {
            if (b==n || (a<n/2 && arr[a] < arr[b])) {
                res[i] = arr[a];
                a++;
            } else {
                res[i] = arr[b];
                b++;
            }
        }
        for (i=0; i<n; i++) {
            arr[i] = res[i];
        }
    }
}

void swap_n_bytes (void *a, void*b, int n)
{
    while (sizeof(int)<=n) {
        n -= sizeof(int);
        int *a_c = (int*)((char*)a+n);
        int *b_c = (int*)((char*)b+n);
        *a_c = *a_c^*b_c;
        *b_c = *a_c^*b_c;
        *a_c = *a_c^*b_c;
    }

    if (n<0) {
        n += sizeof(int);
        while (n<0) {
            n--;
            char *a_c = (char*)a+n;
            char *b_c = (char*)b+n;
            *a_c = *a_c^*b_c;
            *b_c = *a_c^*b_c;
            *a_c = *a_c^*b_c;
        }
    }
}

// Templetized merge sort
// IS_A_LT_B is an expression where *a and *b are pointers
// to _arr_ true when *a<*b.
#define templ_sort(FUNCNAME,TYPE,IS_A_LT_B)                     \
void FUNCNAME ## _user_data (TYPE *arr, int n, void *user_data) \
{                                                               \
    if (n==1) {                                                 \
        return;                                                 \
    } else if (n == 2) {                                        \
        TYPE *a = &arr[1];                                      \
        TYPE *b = &arr[0];                                      \
        int c = IS_A_LT_B;                                      \
        if (c) {                                                \
            swap_n_bytes (&arr[0], &arr[1], sizeof(TYPE));      \
        }                                                       \
    } else {                                                    \
        TYPE res[n];                                            \
        FUNCNAME ## _user_data (arr, n/2, user_data);           \
        FUNCNAME ## _user_data (&arr[n/2], n-n/2, user_data);   \
                                                                \
        int i;                                                  \
        int h=0;                                                \
        int k=n/2;                                              \
        for (i=0; i<n; i++) {                                   \
            TYPE *a = &arr[h];                                  \
            TYPE *b = &arr[k];                                  \
            if (k==n || (h<n/2 && (IS_A_LT_B))) {               \
                res[i] = arr[h];                                \
                h++;                                            \
            } else {                                            \
                res[i] = arr[k];                                \
                k++;                                            \
            }                                                   \
        }                                                       \
        for (i=0; i<n; i++) {                                   \
            arr[i] = res[i];                                    \
        }                                                       \
    }                                                           \
}                                                               \
\
void FUNCNAME(TYPE *arr, int n) {                               \
    FUNCNAME ## _user_data (arr,n,NULL);                        \
}

void sorted_array_print (int *arr, int n)
{
    int sorted[n];
    memcpy (sorted, arr, n*sizeof(int));
    int_sort (sorted, n);
    array_print (sorted, n);
}

// Function that returns an integer in [min,max] with a uniform distribution.
// NOTE: Remember to call srand() ONCE before using this.
#define rand_int_max(max) rand_int_range(0,max)
uint32_t rand_int_range (uint32_t min, uint32_t max)
{
    assert (max < RAND_MAX);
    assert (min < max);
    uint32_t res;
    uint32_t valid_range = RAND_MAX-(RAND_MAX%(max-min+1));
    do {
        res = rand ();
    } while (res >= valid_range);
    return min + res/(valid_range/(max-min+1));
}

// NOTE: Remember to call srand() ONCE before using this.
void fisher_yates_shuffle (int *arr, int n)
{
    int i;
    for (i=n-1; i>0; i--) {
        int j = rand_int_max (i);
        swap (arr+i, arr+j);
    }
}

// NOTE: Remember to call srand() ONCE before using this.
void init_random_array (int *arr, int size)
{
    int i;
    for (i=0; i<size; i++) {
        arr[i] = i;
    }
    fisher_yates_shuffle (arr, size);
}

// TODO: Make this zero initialized in all cases
typedef struct {
    uint32_t size;
    uint32_t len;
    int *data;
} int_dyn_arr_t;

void int_dyn_arr_init (int_dyn_arr_t *arr, uint32_t size)
{
    arr->data = malloc (size * sizeof (*arr->data));
    if (!arr->data) {
        printf ("Malloc failed.\n");
    }
    arr->len = 0;
    arr->size = size;
}

void int_dyn_arr_destroy (int_dyn_arr_t *arr)
{
    free (arr->data);
    *arr = (int_dyn_arr_t){0};
}

void int_dyn_arr_grow (int_dyn_arr_t *arr, uint32_t new_size)
{
    assert (new_size < UINT32_MAX);
    int *new_data;
    if ((new_data = realloc (arr->data, new_size * sizeof(*arr->data)))) {
        arr->data = new_data;
        arr->size = new_size;
    } else {
        printf ("Error: Realloc failed.\n");
        return;
    }
}

void int_dyn_arr_append (int_dyn_arr_t *arr, int element)
{
    if (arr->size == 0) {
        int_dyn_arr_init (arr, 100);
    }

    if (arr->len == arr->size) {
        int_dyn_arr_grow (arr, 2*arr->size);
    }
    arr->data[arr->len++] = element;
}

void int_dyn_arr_insert_and_swap (int_dyn_arr_t *arr, uint32_t pos, int element)
{
    int_dyn_arr_append (arr, element);
    swap (&arr->data[pos], &arr->data[arr->len-1]);
}

void int_dyn_arr_insert_and_shift (int_dyn_arr_t *arr, uint32_t pos, int element)
{
    assert (pos < arr->len);
    if (arr->len == arr->size) {
        int_dyn_arr_grow (arr, 2*arr->size);
    }
    
    int i;
    for (i=arr->len; i>pos; i--) {
        arr->data[i] = arr->data[i-1];
    }
    arr->data[pos] = element;
    arr->len++;
}

void int_dyn_arr_insert_multiple_and_shift (int_dyn_arr_t *arr, uint32_t pos, int *elements, int len)
{
    assert (pos < arr->len);
    if (arr->len + len > arr->size) {
        int new_size = arr->size;
        while (new_size < arr->len + len) {
            new_size *= 2;
        }
        int_dyn_arr_grow (arr, new_size);
    }

    int i;
    for (i=arr->len+len-1; i>pos+len-1; i--) {
        arr->data[i] = arr->data[i-len];
    }

    for (i=0; i<len; i++) {
        arr->data[pos] = elements[i];
        pos++;
    }
    arr->len += len;
}

void int_dyn_arr_print (int_dyn_arr_t *arr)
{
    array_print (arr->data, arr->len);
}

// This is a simple contiguous buffer, arbitrary sized objects are pushed and
// it grows automatically doubling it's previous size.
//
// NOTE: We use zero is initialization unless min_size is used
// WARNING: Don't create pointers into this structure because they won't work
// after a realloc happens.
#define CONT_BUFF_MIN_SIZE 1024
typedef struct {
    uint32_t min_size;
    uint32_t size;
    uint32_t used;
    void *data;
} cont_buff_t;

void* cont_buff_push (cont_buff_t *buff, int size)
{
    if (buff->used + size >= buff->size) {
        void *new_data;
        if (buff->size == 0) {
            int new_size = MAX (CONT_BUFF_MIN_SIZE, buff->min_size);
            if ((buff->data = malloc (new_size))) {
                buff->size = new_size;
            } else {
                printf ("Malloc failed.\n");
            }
        } else if ((new_data = realloc (buff->data, 2*buff->size))) {
            buff->data = new_data;
            buff->size = 2*buff->size;
        } else {
            printf ("Error: Realloc failed.\n");
            return NULL;
        }
    }

    void *ret = (uint8_t*)buff->data + buff->used;
    buff->used += size;
    return ret;
}

// NOTE: The same cont_buff_t can be used again after this.
void cont_buff_destroy (cont_buff_t *buff)
{
    free (buff->data);
    buff->data = NULL;
    buff->size = 0;
    buff->used = 0;
}

// Memory pool that grows as needed, and can be freed easily.
#define MEM_POOL_MIN_BIN_SIZE 1024
typedef struct {
    uint32_t min_bin_size;
    uint32_t size;
    uint32_t used;
    void *base;

    uint32_t total_used;
    uint32_t num_bins;
} mem_pool_t;

struct _bin_info_t {
    void *base;
    uint32_t size;
    struct _bin_info_t *prev_bin_info;
};

typedef struct _bin_info_t bin_info_t;

enum alloc_opts {
    POOL_UNINITIALIZED,
    POOL_ZERO_INIT
};

// pom == pool or malloc
#define pom_push_struct(pool, type) pom_push_size(pool, sizeof(type))
#define pom_push_array(pool, n, type) pom_push_size(pool, (n)*sizeof(type))
#define pom_push_size(pool, size) (pool==NULL? malloc(size) : mem_pool_push_size(pool,size))

#define mem_pool_push_struct(pool, type) mem_pool_push_size(pool, sizeof(type))
#define mem_pool_push_array(pool, n, type) mem_pool_push_size(pool, (n)*sizeof(type))
#define mem_pool_push_size(pool, size) mem_pool_push_size_full(pool, size, POOL_UNINITIALIZED)
void* mem_pool_push_size_full (mem_pool_t *pool, int size, enum alloc_opts opts)
{
    if (pool->used + size >= pool->size) {
        pool->num_bins++;
        int new_bin_size = MAX (MAX (MEM_POOL_MIN_BIN_SIZE, pool->min_bin_size), size);
        void *new_bin;
        bin_info_t *new_info;
        if ((new_bin = malloc (new_bin_size + sizeof(bin_info_t)))) {
            new_info = (bin_info_t*)((uint8_t*)new_bin + new_bin_size);
        } else {
            printf ("Malloc failed.\n");
            return NULL;
        }

        new_info->base = new_bin;
        new_info->size = new_bin_size;

        if (pool->base == NULL) {
            new_info->prev_bin_info = NULL;
        } else {
            bin_info_t *prev_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
            new_info->prev_bin_info = prev_info;
        }

        pool->used = 0;
        pool->size = new_bin_size;
        pool->base = new_bin;
    }

    void *ret = (uint8_t*)pool->base + pool->used;
    pool->used += size;
    pool->total_used += size;

    if (opts == POOL_ZERO_INIT) {
        memset (ret, 0, size);
    }
    return ret;
}

// NOTE: _pool_ can be reused after this.
void mem_pool_destroy (mem_pool_t *pool)
{
    if (pool->base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
        while (curr_info->prev_bin_info != NULL) {
            void *to_free = curr_info->base;
            curr_info = curr_info->prev_bin_info;
            free (to_free);
        }
        free (curr_info->base);
        pool->base = NULL;
    }
    pool->size = 0;
    pool->used = 0;
    pool->total_used = 0;
    pool->num_bins = 0;
}

uint32_t mem_pool_allocated (mem_pool_t *pool)
{
    uint64_t allocated = 0;
    if (pool->base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)pool->base + pool->size);
        while (curr_info != NULL) {
            allocated += curr_info->size + sizeof(bin_info_t);
            curr_info = curr_info->prev_bin_info;
        }
    }
    return allocated;
}

void mem_pool_print (mem_pool_t *pool)
{
    uint32_t allocated = mem_pool_allocated(pool);
    printf ("Allocated: %u bytes\n", allocated);
    printf ("Available: %u bytes\n", pool->size-pool->used);
    printf ("Used: %u bytes (%.2f%%)\n", pool->total_used, ((double)pool->total_used*100)/allocated);
    uint64_t info_size = pool->num_bins*sizeof(bin_info_t);
    printf ("Info: %lu bytes (%.2f%%)\n", info_size, ((double)info_size*100)/allocated);

    // NOTE: This is the amount of space left empty in previous bins
    uint64_t left_empty;
    if (pool->num_bins>0)
        left_empty = (allocated - pool->size - sizeof(bin_info_t))- /*allocated except last bin*/
                     (pool->total_used - pool->used)-               /*total_used except last bin*/
                     (pool->num_bins-1)*sizeof(bin_info_t);         /*size used in bin_info_t*/
    else {
        left_empty = 0;
    }
    printf ("Left empty: %lu bytes (%.2f%%)\n", left_empty, ((double)left_empty*100)/allocated);
    printf ("Bins: %u\n", pool->num_bins);
}

typedef struct {
    mem_pool_t *pool;
    void* base;
    uint32_t used;
    uint32_t total_used;
} pool_temp_marker_t;

pool_temp_marker_t mem_pool_begin_temporary_memory (mem_pool_t *pool)
{
    pool_temp_marker_t res;
    res.total_used = pool->total_used;
    res.used = pool->used;
    res.base = pool->base;
    res.pool = pool;
    return res;
}

void mem_pool_end_temporary_memory (pool_temp_marker_t mrkr)
{
    if (mrkr.base != NULL) {
        bin_info_t *curr_info = (bin_info_t*)((uint8_t*)mrkr.pool->base + mrkr.pool->size);
        while (curr_info->base != mrkr.base) {
            void *to_free = curr_info->base;
            curr_info = curr_info->prev_bin_info;
            free (to_free);
            mrkr.pool->num_bins--;
        }
        mrkr.pool->size = curr_info->size;
        mrkr.pool->base = curr_info->base;
        mrkr.pool->used = mrkr.used;
        mrkr.pool->total_used = mrkr.total_used;
    } else {
        // NOTE: Here mrkr was created before the pool was initialized, so we
        // destroy everything.
        mem_pool_destroy (mrkr.pool);
    }
}

// Flatten an array of null terminated strings into a single string allocated
// into _pool_ or heap.
char* collapse_str_arr (char **arr, int n, mem_pool_t *pool)
{
    int len = 0;
    int i;
    for (i=0; i<n; i++) {
        len += strlen (arr[i]) + 1;
    }
    char *res = pom_push_size (pool, len);

    char *ptr = res;
    for (i=0; i<n; i++) {
        ptr = stpcpy (ptr, arr[i]);
        *ptr = ' ';
        ptr++;
    }
    ptr--;
    *ptr = '\0';
    return res;
}

// Expand _str_ as bash would, allocate it in _pool_ or heap. 
// NOTE: $(<cmd>) and `<cmd>` work but don't get too crazy, this spawns /bin/sh
// and a subprocess. Using env vars like $HOME, or ~/ doesn't.
char* sh_expand (char *str, mem_pool_t *pool)
{
    wordexp_t out;
    wordexp (str, &out, 0);
    char *res = collapse_str_arr (out.we_wordv, out.we_wordc, pool);
    wordfree (&out);
    return res;
}

#define COMMON_H
#endif
