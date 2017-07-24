#if !defined(COMMON_H)
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
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
#define templ_sort(FUNCNAME,TYPE,IS_A_LT_B)                 \
void FUNCNAME (TYPE *arr, int n)                            \
{                                                           \
    if (n==1) {                                             \
        return;                                             \
    } else if (n == 2) {                                    \
        TYPE *a = &arr[1];                                  \
        TYPE *b = &arr[0];                                  \
        int c = IS_A_LT_B;                                  \
        if (c) {                                            \
            swap_n_bytes (&arr[0], &arr[1], sizeof(TYPE));  \
        }                                                   \
    } else {                                                \
        TYPE res[n];                                        \
        FUNCNAME (arr, n/2);                                \
        FUNCNAME (&arr[n/2], n-n/2);                        \
                                                            \
        int i;                                              \
        int h=0;                                            \
        int k=n/2;                                          \
        for (i=0; i<n; i++) {                               \
            TYPE *a = &arr[h];                              \
            TYPE *b = &arr[k];                              \
            int c = IS_A_LT_B;                              \
            if (k==n || (h<n/2 && c)) {                     \
                res[i] = arr[h];                            \
                h++;                                        \
            } else {                                        \
                res[i] = arr[k];                            \
                k++;                                        \
            }                                               \
        }                                                   \
        for (i=0; i<n; i++) {                               \
            arr[i] = res[i];                                \
        }                                                   \
    }                                                       \
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

void init_random_array (int *arr, int size)
{
    int i;
    for (i=0; i<size; i++) {
        arr[i] = i;
    }
    srand (time(NULL));
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

#define mem_pool_push_struct(pool, type) mem_pool_push_size(pool, sizeof(type))
#define mem_pool_push_array(pool, n, type) mem_pool_push_size(pool, (n)*sizeof(type))
#define mem_pool_push_size(pool, size) mem_pool_push_size_full(pool, size, POOL_UNINITIALIZED);
void* mem_pool_push_size_full (mem_pool_t *pool, int size, enum alloc_opts opts)
{
    if (pool->used + size >= pool->size) {
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
}

typedef struct {
    mem_pool_t *pool;
    void* base;
    uint32_t used;
} pool_temp_marker_t;

pool_temp_marker_t mem_pool_begin_temporary_memory (mem_pool_t *pool)
{
    pool_temp_marker_t res;
    res.used = pool->used;
    res.base = pool->base;
    res.pool = pool;
    return res;
}

// FIXME: Temporary memory fails if nothing has been allocated before on the
// pool, curr_info == NULL.
void mem_pool_end_temporary_memory (pool_temp_marker_t mrkr)
{
    bin_info_t *curr_info = (bin_info_t*)((uint8_t*)mrkr.pool->base + mrkr.pool->size);
    while (curr_info->base != mrkr.base) {
        void *to_free = curr_info->base;
        curr_info = curr_info->prev_bin_info;
        free (to_free);
    }
    mrkr.pool->size = curr_info->size;
    mrkr.pool->base = curr_info->base;
    mrkr.pool->used = mrkr.used;
}

#define COMMON_H
#endif
