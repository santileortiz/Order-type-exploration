#if !defined(COMMON_H)
#include <inttypes.h>
#include <assert.h>
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
    double E[4];
} vect4_t;
#define VECT4(x,y,z,w) ((vect4_t){{x,y,z,w}})

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

void swap (int*a, int*b)
{
    *a = *a^*b;
    *b = *a^*b;
    *a = *a^*b;
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

#define COMMON_H
#endif
