/*
 * Copiright (C) 2017 Santiago León O.
 */

#if !defined(SEQUENCE_STORE_H)
#define SEQUENCE_STORE_H

struct backtrack_node_t {
    int id;
    int val;
    int num_children;
    struct backtrack_node_t *children[1];
};

enum sequence_file_type_t {
    SEQ_FIXED_LEN           = 1L<<1,
    SEQ_TIMING              = 1L<<2,
};

enum sequence_stor_options_t {
    SEQ_DEFAULT             = 0,
    // Enables capturing of extra information about the search, while disabling
    // all allocations of the result.
    SEQ_DRY_RUN             = 1L<<1,
};

struct sequence_store_t;

// Callback type that can be called for each sequence:
//  _stor_: sequence_store_t being used (provided for weird usecases).
//  _seq_: Values of the nodes of the path to the leave (excluding root node).
//  _len_: Size of _seq_.
//  _closure_: Used to send data to the callback from seq_set_callback()'s call.
//
//  Use seq_set_callback() to enable one over a sequence_store_t.
//  NOTE: DO NOT! mutate _seq_ will break everything.
#define SEQ_CALLBACK(name) void name(struct sequence_store_t *stor, int *seq, int len, void* closure)
typedef SEQ_CALLBACK(seq_callback_t);

// Simple example callback.
SEQ_CALLBACK(seq_print_callback)
{
    array_print (seq, len);
}

struct sequence_store_t {
    enum sequence_file_type_t type;
    enum sequence_stor_options_t opts;
    int file;
    char *filename;
    uint32_t custom_file_header_size;
    mem_pool_t *pool;

    struct timespec begin;
    struct timespec end;
    float time;

    uint32_t sequence_size;
    uint32_t num_sequences;
    uint32_t max_sequences;
    int_dyn_arr_t dyn_arr;
    int *seq;

    // Tree data
    struct backtrack_node_t *tree_root;
    uint32_t max_len; // Height of the tree
    uint32_t max_children;
    uint32_t max_node_size;
    uint32_t num_nodes_stack; // num_nodes_in_stack
    struct backtrack_node_t *node_stack;
    uint64_t num_nodes;
    mem_pool_t temp_pool;

    int64_t last_l;

    // Attributes used for the callback
    // NOTE: If SEQ_DRY_RUN is used, we store the values of the sequence here.
    // Otherwise we get them from sequence_store_t->node_stack.
    int *sequence_values;
    void *closure;
    seq_callback_t *callback;
    uint32_t callback_max_num_sequences;
    uint32_t callback_sequence_len;
    uint32_t callback_num_sequences; // Number of sequences of length callback_sequence_len

    // Optional information about the search
    uint32_t final_max_len;
    uint32_t final_max_children;
    uint64_t expected_tree_size;
    uint64_t *nodes_per_len; // Allocated in sequence_store_t->pool
    uint64_t *leaves_per_len; // Allocated in sequence_store_t->pool
    int *children_count_stack; // Used to compute expected_tree_size, allocated in sequence_store_t->temp_pool
    int num_children_count_stack;
    // TODO: Implement the following info:
    // uint64_t expected_sequence_size;
};

#define new_sequence_store(filename, pool) new_sequence_store_opts(filename, pool, SEQ_DEFAULT)
struct sequence_store_t new_sequence_store_opts (char *filename, mem_pool_t *pool,
                                                 enum sequence_stor_options_t opts);

void seq_set_callback (struct sequence_store_t *stor, seq_callback_t *callback, void *closure);
void seq_set_seq_number (struct sequence_store_t *stor, int num_sequences);
void seq_set_seq_len (struct sequence_store_t *stor, int len);
bool seq_finish (struct sequence_store_t *stor);

uint32_t backtrack_node_size (int num_children);
void seq_push_element (struct sequence_store_t *stor, int val, int64_t level);
void seq_tree_extents (struct sequence_store_t *stor, uint32_t max_children, uint32_t max_len);
struct backtrack_node_t* seq_tree_end (struct sequence_store_t *stor);

enum tree_print_mode_t {
    TREE_PRINT_LEN,
    TREE_PRINT_FULL
};
#define seq_tree_print_sequences(root,len) \
    seq_tree_print_sequences_full(root,len,array_print,TREE_PRINT_LEN)
#define seq_tree_print_all_sequences(root,len) \
    seq_tree_print_sequences_full(root,len,array_print,TREE_PRINT_FULL)
void seq_tree_print_sequences_full (struct backtrack_node_t *root, int len,
                                    int_arr_print_callback_t *print_func,
                                    enum tree_print_mode_t mode);

uint64_t* get_nodes_per_len (struct backtrack_node_t *n, mem_pool_t *pool, int len);
void seq_timing_begin (struct sequence_store_t *stor);
void seq_timing_end (struct sequence_store_t *stor);

struct file_header_t {
    enum sequence_file_type_t type;
    uint32_t custom_header_size;
    uint32_t sequence_size;
    uint32_t num_sequences;
    float time;
};

void seq_add_file_header (struct sequence_store_t *stor, void *header, uint32_t size);
void seq_allocate_file_header (struct sequence_store_t *stor, uint32_t size);
#define seq_write_file_header(stor,header) seq_add_file_header(stor,header,0)

int *seq_read_file (char *filename, mem_pool_t *pool, struct file_header_t *header, void *custom_header);

void seq_set_length (struct sequence_store_t *stor, uint32_t sequence_size, uint32_t max_sequences);
#define seq_push_sequence(store,seq) seq_push_sequence_size(store,seq,0)
void seq_push_sequence_size (struct sequence_store_t *stor, int *seq, uint32_t size);
int* seq_end (struct sequence_store_t *stor);

void seq_print_info (struct sequence_store_t *stor);
#ifdef CAIRO_PDF_H
void seq_tree_draw (char* fname, struct sequence_store_t *stor, double width,
                    double ar, double line_width, double min_line_width, double node_r);
#endif /*CAIRO_PDF_H*/

#endif /*SEQUENCE_STORE_H*/

#ifdef SEQUENCE_STORE_IMPL
#undef SEQUENCE_STORE_IMPL

// Sets up _callback_ to be called with _closure_ in it's arguments every time
// we reach a leave on the tree (every time we finish a sequence).
// NOTE: see SEQ_CALLBACK() macro for info on the seq_callback_t type.
void seq_set_callback (struct sequence_store_t *stor,
                       seq_callback_t *callback, void *closure)
{
    stor->callback = callback;
    stor->closure = closure;
}

// Limits the number of sequences on which the previous callback is called, if
// the algorithm wants it, it can use seq_finish() to maybe break earlier from
// the algorithm.
void seq_set_seq_number (struct sequence_store_t *stor,
                         int num_sequences)
{
    stor->callback_max_num_sequences = num_sequences;
}

void seq_set_seq_len (struct sequence_store_t *stor, int len)
{
    stor->callback_sequence_len = len;
}

// Can be used by the algorithm after adding something to the store if the user
// has configured some break condition (like the number of sequences above).
bool seq_finish (struct sequence_store_t *stor)
{
    if (stor->callback_max_num_sequences != 0 &&
        stor->callback_num_sequences >= stor->callback_max_num_sequences) {
        return true;
    }
    return false;
}

struct backtrack_node_t* stack_element (struct sequence_store_t *stor, uint32_t i)
{
    struct backtrack_node_t *res = (struct backtrack_node_t*)((char*)stor->node_stack + (i)*stor->max_node_size);
    assert (stor->node_stack != NULL);
    assert (res <= (struct backtrack_node_t*)((char*)stor->node_stack +
                                       (stor->max_len+1)*stor->max_node_size));
    return (res);
}

void push_partial_node (struct sequence_store_t *stor, int val)
{
    struct backtrack_node_t *node = stack_element (stor, stor->num_nodes_stack);
    stor->num_nodes_stack++;

    *node = (struct backtrack_node_t){0};
    node->val = val;
    node->id = stor->num_nodes;
    stor->num_nodes++;
}

uint32_t backtrack_node_size (int num_children)
{
    uint32_t node_size;
    if (num_children > 1) {
        node_size = sizeof(struct backtrack_node_t)+(num_children-1)*sizeof(struct backtrack_node_t*);
    } else {
        node_size = sizeof(struct backtrack_node_t);
    }
    return node_size;
}

struct backtrack_node_t* complete_and_pop_node (struct sequence_store_t *stor, int64_t l)
{
    struct backtrack_node_t *finished = stack_element (stor, l+1);
    uint32_t node_size = backtrack_node_size (finished->num_children);
    struct backtrack_node_t *pushed_node = mem_pool_push_size (stor->pool, node_size);
    pushed_node->id = finished->id;
    pushed_node->val = finished->val;
    pushed_node->num_children = finished->num_children;
    int i;
    for (i=0; i<finished->num_children; i++) {
        pushed_node->children[i] = finished->children[i];
    }

    stor->num_nodes_stack--;

    if (l >= 0) {
        struct backtrack_node_t *parent = stack_element (stor, l);
        parent->children[parent->num_children] = pushed_node;
        parent->num_children++;
        l--;
    }
    return pushed_node;
}

void seq_dry_run_call_callback (struct sequence_store_t *stor, int val, int level)
{
    if (stor->last_l >= level) {
        stor->num_sequences++;
        if (stor->callback_max_num_sequences == 0 ||
            stor->callback_num_sequences < stor->callback_max_num_sequences) {
            if (stor->callback_sequence_len == 0 ||
                stor->last_l+1 == stor->callback_sequence_len) {
                stor->callback_num_sequences++;

                if (stor->callback != NULL) {
                    stor->callback (stor, stor->sequence_values, stor->last_l+1, stor->closure);
                }
            }
        }
        if (stor->leaves_per_len != NULL) {
            stor->leaves_per_len[stor->last_l+1]++;
        }
    }

    if (stor->callback != NULL) {
        if (level > -1) {
            stor->sequence_values[level] = val;
        }
    }
}

void seq_normal_call_callback (struct sequence_store_t *stor, int level)
{
    if (stor->last_l >= level) {
        stor->num_sequences++;
        if (stor->callback_max_num_sequences == 0 ||
            stor->callback_num_sequences <= stor->callback_max_num_sequences) {
            if (stor->callback_sequence_len == 0 ||
                stor->last_l+1 == stor->callback_sequence_len) {
                stor->callback_num_sequences++;
                if (stor->callback != NULL ) {
                    int curr_sequence[stor->num_nodes_stack];
                    int i;
                    for (i=1; i<stor->num_nodes_stack; i++) {
                        struct backtrack_node_t *node = stack_element (stor, i);
                        curr_sequence[i-1] = node->val;
                    }
                    stor->callback (stor, curr_sequence, stor->num_nodes_stack-1, stor->closure);
                }
            }
        }
    }
}

void seq_push_element (struct sequence_store_t *stor,
                       int val, int64_t level)
{
    if (stor->callback_max_num_sequences != 0 &&
        stor->callback_num_sequences > stor->callback_max_num_sequences) {
        return;
    }

    stor->final_max_len = MAX (stor->final_max_len, level+1);

    if (stor->opts & SEQ_DRY_RUN) {
        stor->num_nodes++;
        if (stor->nodes_per_len != NULL) {
            stor->nodes_per_len[level+1]++;
        }

        seq_dry_run_call_callback (stor, val, level);

        if (!seq_finish (stor)) {
            while (stor->last_l >= level) {
                assert (stor->last_l >= 0);
                uint32_t final_children_count = stor->children_count_stack[stor->last_l+1];
                stor->final_max_children = MAX (stor->final_max_children, final_children_count);
                stor->expected_tree_size += backtrack_node_size (final_children_count);
                stor->num_children_count_stack--;
                if (stor->last_l >= 0) {
                    stor->children_count_stack[stor->last_l]++;
                }
                stor->last_l--;
            }

            assert (stor->last_l + 1 == level
                    && "Nodes should be pushed with level increasing by 1");
            stor->last_l = level;
            stor->children_count_stack[stor->num_children_count_stack] = 0;
            stor->num_children_count_stack++;
        }

        // NOTE: Why would someone want to compute tree information while also
        // computing the full tree?, if this is an actual usecase, then we don't
        // really want to return here. CAREFUL: stor->last_l will be used from
        // two places.
        return;
    }

    seq_normal_call_callback (stor, level);

    if (!seq_finish (stor)) {
        while (stor->last_l >= level) {
            assert (stor->last_l >= 0);
            complete_and_pop_node (stor, stor->last_l);
            stor->last_l--;
        }
        assert (stor->last_l + 1 == level
                && "Nodes should be pushed with level increasing by 1");
        stor->last_l = level;
        push_partial_node (stor, val);
    }
}

void seq_tree_extents (struct sequence_store_t *stor, uint32_t max_children, uint32_t max_len)
{
    if (max_children > 0) {
        stor->max_children = max_children;
        stor->max_node_size =
            (sizeof(struct backtrack_node_t) +
             (stor->max_children-1)*sizeof(struct backtrack_node_t*));
    } else {
        // TODO: If we don't know max_children then use a version of
        // struct backtrack_node_t that has a cont_buff_t as children. Remember to
        // compute max_node_size in this case too.
        invalid_code_path;
    }

    if (max_len > 0) {
        stor->max_len = max_len;
        stor->node_stack =
            mem_pool_push_size (&stor->temp_pool,(stor->max_len+1)*stor->max_node_size);

        if (stor->opts & SEQ_DRY_RUN) {
            if (stor->callback != NULL) {
                stor->sequence_values =
                    mem_pool_push_size (&stor->temp_pool,
                                        (stor->max_len+1)*sizeof(stor->sequence_values));
            }

            stor->children_count_stack =
                mem_pool_push_size_full (&stor->temp_pool,
                                         (stor->max_len+1)*sizeof(stor->children_count_stack),
                                         POOL_ZERO_INIT);
            if (stor->pool != NULL) {
                stor->nodes_per_len =
                    mem_pool_push_size_full (stor->pool,
                                             (stor->max_len+1)*sizeof(*stor->nodes_per_len),
                                             POOL_ZERO_INIT);
                stor->leaves_per_len =
                    mem_pool_push_size_full (stor->pool,
                                             (stor->max_len+1)*sizeof(*stor->nodes_per_len),
                                             POOL_ZERO_INIT);
            }
        }
    } else {
        // TODO: If we don't know max_len then node_stack should be a
        // cont_buff_t of elements of size max_node_size.
        invalid_code_path;
    }
    // Pushing the root node to the stack.
    seq_push_element (stor, -1, -1);
}

struct backtrack_node_t* seq_tree_end (struct sequence_store_t *stor)
{
    if (!(stor->opts & SEQ_DRY_RUN)) {
        seq_normal_call_callback (stor, -1);

        while (stor->last_l > -1) {
            complete_and_pop_node (stor, stor->last_l);
            stor->last_l--;
        }
        stor->tree_root = complete_and_pop_node (stor, stor->last_l);
    } else {
        seq_dry_run_call_callback (stor, 0, -1);

        while (stor->last_l >= -1) {
            uint32_t final_children_count = stor->children_count_stack[stor->last_l+1];
            stor->final_max_children = MAX (stor->final_max_children, final_children_count);
            stor->expected_tree_size += backtrack_node_size (final_children_count);
            stor->num_children_count_stack--;
            if (stor->last_l >= 0) {
                stor->children_count_stack[stor->last_l]++;
            }
            stor->last_l--;
        }
        stor->tree_root = NULL;
    }

    mem_pool_destroy (&stor->temp_pool);
    return stor->tree_root;
}

/* Prints all sequences on a backtrack tree of length len. A sequence
 * necessarily ends in a node without children. See examples below.
 *
 *                   R        -1
 *                 / | \
 *                1  2  3      0
 *               /|  |  |\
 *              1 5  4  7 1    1.
 *             / /|    /|\
 *            5 2 9   3 5 2    2
 *
 * seq_tree_print_sequences (R, 1):
 *  <Nothing>
 * seq_tree_print_sequences (R, 2):
 *  2 4
 *  3 1
 * seq_tree_print_sequences (R, 3):
 *  1 1 5
 *  1 5 2
 *  1 5 9
 *  3 7 3
 *  3 7 5
 *  3 7 2
 *
 * The function is actually a macro to seq_tree_print_sequences_full() with some
 * defaults. These are the extra options:
 *
 *    - Function _print_func_ is called to print a sequence (default: array_print()).
 *    - If _mode_ == TREE_PRINT_FULL then also sequences smaller than _len_ will
 *      be printed (default: TREE_PRINT_LEN).
 *
 *      Example:
 *
 *        seq_tree_print_sequences_full (R, 3, array_print, TREE_PRINT_FULL):
 *         1 1 5
 *         1 5 2
 *         1 5 9
 *         2 4
 *         3 7 3
 *         3 7 5
 *         3 7 2
 *         3 1
 */

void seq_tree_print_sequences_full (struct backtrack_node_t *root, int len,
                                    int_arr_print_callback_t *print_func,
                                    enum tree_print_mode_t mode)
{
    if (len <= 0) return;

    int l = 1;
    int seq[len];
    struct backtrack_node_t *seq_nodes[len];
    seq_nodes[0] = root;
    int curr_child_id[len];

    struct backtrack_node_t *curr = root->children[0];
    seq[0] = curr->val;
    curr_child_id[0] = 0;
    curr_child_id[1] = 0;

    while (l>0) {
        if (curr->num_children == 0) {
            if (mode == TREE_PRINT_FULL) {
                print_func (seq, l);
            } else if (l == len) {
                print_func (seq, l);
            }
        }

        while (1) {
            if (curr->num_children != 0 && curr_child_id[l] < curr->num_children) {
                seq_nodes[l] = curr;
                curr = curr->children[curr_child_id[l]];
                seq[l] = curr->val;

                if (l < len-1) {
                    curr_child_id[l+1] = 0;
                }
                l++;
                break;
            }

            l--;
            if (l >= 0) {
                curr = seq_nodes[l];
                curr_child_id[l]++;
            } else {
                break;
            }
        }
    }
}

void get_nodes_per_len_helper (struct backtrack_node_t *n, uint64_t *res, int l)
{
    res[l]++;

    int ch_id;
    for (ch_id=0; ch_id<n->num_children; ch_id++) {
        get_nodes_per_len_helper (n->children[ch_id], res, l+1);
    }
}

// NOTE: len must be equal to seq.final_max_len, there is no check for this in
// the code.
// TODO: Make the above not a necessary condition.
uint64_t* get_nodes_per_len (struct backtrack_node_t *n, mem_pool_t *pool, int len)
{
    uint64_t *res = mem_pool_push_size_full (pool, sizeof(uint64_t)*(len+1), POOL_ZERO_INIT);
    get_nodes_per_len_helper (n, res, 0);
    return res;
}

void seq_timing_begin (struct sequence_store_t *stor)
{
    clock_gettime (CLOCK_MONOTONIC, &stor->begin);
    stor->type |= SEQ_TIMING;
}

void seq_timing_end (struct sequence_store_t *stor)
{
    assert ((stor->type & SEQ_TIMING) && "Call seq_timing_begin() before.");
    clock_gettime (CLOCK_MONOTONIC, &stor->end);
    stor->time = time_elapsed_in_ms (&stor->begin, &stor->end);
}

void seq_allocate_file_header (struct sequence_store_t *stor, uint32_t size)
{
    stor->custom_file_header_size = size;
    lseek (stor->file, sizeof(struct file_header_t)+size, SEEK_SET);
}

int *seq_read_file (char *filename, mem_pool_t *pool, struct file_header_t *header, void *custom_header)
{
    int *res;
    int file = open (filename, O_RDONLY);
    if (file == -1) {
        return NULL;
    } else {
        struct file_header_t local_header;
        struct file_header_t *l_header = (header != NULL) ? header : &local_header;
        file_read (file, l_header, sizeof (struct file_header_t));
        uint32_t size = l_header->sequence_size*l_header->num_sequences*sizeof(int);
        if (pool != NULL) {
            res = mem_pool_push_size (pool, size);
        } else {
            res = malloc (size);
        }

        if (l_header->custom_header_size > 0) {
            if (custom_header != NULL) {
                file_read (file, custom_header, l_header->custom_header_size);
            } else {
                lseek (file, sizeof(struct file_header_t)+l_header->custom_header_size, SEEK_SET);
            }
        }
        file_read (file, res, size);
    }
    return res;
}

void seq_add_file_header (struct sequence_store_t *stor, void *header, uint32_t size)
{
    if (size == 0) {
        assert (stor->custom_file_header_size != 0
                && "Custom header size was not set, call seq_add_file_header() before pushing something.");
    } else {
        stor->custom_file_header_size = size;
    }
    lseek (stor->file, sizeof(struct file_header_t), SEEK_SET);
    file_write (stor->file, header, stor->custom_file_header_size);
}

struct sequence_store_t new_sequence_store_opts (char *filename, mem_pool_t *pool,
                                                 enum sequence_stor_options_t opts)
{
    struct sequence_store_t res = {0};
    res.opts = opts;
    res.last_l = -2;
    if (filename != NULL) {
        remove (filename);
        res.filename = filename;
        res.file = open (filename, O_RDWR|O_CREAT, 0666);
        lseek (res.file, sizeof (struct file_header_t), SEEK_SET);
    } else {
        res.file  = -1;
    }

    if (pool != NULL) {
        res.pool = pool;
    }
    return res;
}

void seq_set_length (struct sequence_store_t *stor, uint32_t sequence_size, uint32_t max_sequences)
{
    if (sequence_size > 0) {
        stor->type |= SEQ_FIXED_LEN;
        stor->sequence_size = sequence_size;
        if (max_sequences > 0) {
            stor->max_sequences = max_sequences;
            stor->seq = mem_pool_push_size (stor->pool, sizeof(int)*sequence_size*max_sequences);
        }
    }
}

#define seq_push_sequence(store,seq) seq_push_sequence_size(store,seq,0)
void seq_push_sequence_size (struct sequence_store_t *stor, int *seq, uint32_t size)
{
    if (stor->pool == NULL && stor->filename == NULL) {
        // NOTE: There is no set destination, print to stdout.
        size = (size == 0) ? stor->sequence_size : size;
        array_print (seq, size);
        return;
    }

    if (size == 0) {
        // NOTE: Fixed length sequence.
        assert (stor->sequence_size != 0
                && "Sequence size not specified but store has no fixed size.");
        if (stor->pool != NULL) {
            // NOTE: RAM memory as output is used.
            if (stor->seq == NULL) {
                // NOTE: Number of sequences is unknown, store->seq not allocated.
                assert (stor->max_sequences == 0);
                int i;
                for (i=0; i<stor->sequence_size; i++) {
                    int_dyn_arr_append (&stor->dyn_arr, seq[i]);
                }
            } else {
                if (stor->num_sequences < stor->max_sequences) {
                    int i;
                    for (i=0; i<stor->sequence_size; i++) {
                        stor->seq[stor->num_sequences*stor->sequence_size+i] = seq[i];
                    }
                } else {
                    static bool print_once = false;
                    if (!print_once) {
                        printf("Adding more sequences than max_sequences.");
                        print_once = true;
                    }
                }
            }
        }

        if (stor->filename != NULL) {
            // NOTE: File as output.
            file_write (stor->file, seq, stor->sequence_size*sizeof(int));
        }
        stor->num_sequences++;
    } else {
        //TODO: Implement tree behavior here.
    }

    if (stor->callback != NULL) {
        stor->callback (stor, seq, size, stor->closure);
    }
}

int* seq_end (struct sequence_store_t *stor)
{
    if (stor->pool != NULL &&
        stor->seq == NULL && stor->sequence_size != 0 && stor->max_sequences == 0) {
        // NOTE: Fixed size sequence but unknown limit on number of sequences.
        uint32_t bytes = sizeof(int)*stor->dyn_arr.len;
        stor->seq = mem_pool_push_size (stor->pool, bytes);
        memcpy (stor->seq, stor->dyn_arr.data, bytes);
        int_dyn_arr_destroy (&stor->dyn_arr);
    }

    if (stor->file) {
        struct file_header_t header = {0};
        header.type = stor->type;
        header.custom_header_size = stor->custom_file_header_size;
        if (stor->type & SEQ_TIMING) {
            header.time = stor->time;
        }
        if (stor->type & SEQ_FIXED_LEN) {
            header.sequence_size = stor->sequence_size;
        }
        header.num_sequences = stor->num_sequences;
        lseek (stor->file, 0, SEEK_SET);
        file_write (stor->file, &header, sizeof (struct file_header_t));
        close (stor->file);
    }

    return stor->seq;
}

void seq_print_info (struct sequence_store_t *stor)
{
    uint32_t h = stor->final_max_len;
    printf ("Levels: %d + root\n", h);
    printf ("Nodes: %"PRIu64" + root\n", stor->num_nodes-1);
    if (stor->nodes_per_len != NULL) {
        printf ("Nodes per level: ");
        print_u64_array (stor->nodes_per_len, stor->final_max_len+1);
    }
    printf ("Sequences (leaves): %"PRIu32"\n", stor->num_sequences);
    if (stor->leaves_per_len != NULL) {
        printf ("Sequences per level: ");
        print_u64_array (stor->leaves_per_len, stor->final_max_len+1);
    }
    printf ("Tree size: %"PRIu64" bytes\n", stor->expected_tree_size);
    printf ("Max children: %u\n", stor->final_max_children);

    if (stor->time != 0) {
        printf ("Time: %f ms\n", stor->time);
    }
}

#ifdef CAIRO_PDF_H
// The followng is an implementation of the algorithm developed in [1] to draw
// trees in linear time.
//
// [1] Buchheim, C., Jünger, M. and Leipert, S. (2006), Drawing rooted trees in
// linear time. Softw: Pract. Exper., 36: 651–665. doi:10.1002/spe.713
struct _layout_tree_node_t {
    double mod;
    double prelim;
    double change;
    double shift;
    double width;
    uint32_t node_id;
    struct _layout_tree_node_t *parent;
    struct _layout_tree_node_t *ancestor;
    struct _layout_tree_node_t *thread;
    vect2_t pos;
    uint32_t child_id; // position among its siblings
    uint32_t num_children;
    struct _layout_tree_node_t *children[1];
};
typedef struct _layout_tree_node_t layout_tree_node_t;

#define push_layout_node(buff, num_children) (num_children)>1? \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t) + \
                        (num_children-1)*sizeof(layout_tree_node_t*)) : \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t))

#define create_layout_tree(pool,sep,node) \
    create_layout_tree_helper(pool,sep,node,NULL,0)
layout_tree_node_t* create_layout_tree_helper (mem_pool_t *pool,
                                               double h_separation,
                                               struct backtrack_node_t *node,
                                               layout_tree_node_t *parent, uint32_t child_id)
{
    layout_tree_node_t *lay_node = push_layout_node (pool, node->num_children);
    *lay_node = (layout_tree_node_t){0};
    lay_node->ancestor = lay_node;
    lay_node->parent = parent;
    lay_node->child_id = child_id;
    lay_node->node_id = node->id;
    lay_node->num_children = node->num_children;

    lay_node->width = h_separation;

    int ch_id;
    for (ch_id=0; ch_id<node->num_children; ch_id++) {
        lay_node->children[ch_id] =
            create_layout_tree_helper (pool, h_separation,
                                       node->children[ch_id], lay_node, ch_id);
    }

    return lay_node;
}

#define rightmost(v) (v->children[v->num_children-1])
#define leftmost(v) (v->children[0])
#define left_sibling(v) (v->parent->children[v->child_id-1])

layout_tree_node_t* tree_layout_next_left (layout_tree_node_t *v)
{
    if (v->num_children > 0) {
        return leftmost(v);
    } else {
        return v->thread;
    }
}

layout_tree_node_t* tree_layout_next_right (layout_tree_node_t *v)
{
    if (v->num_children > 0) {
        return rightmost(v);
    } else {
        return v->thread;
    }
}

void tree_layout_move_subtree (layout_tree_node_t *w_m, layout_tree_node_t *w_p, double shift)
{
    double subtrees = w_p->child_id - w_m->child_id;
    w_p->change -= shift/subtrees;
    w_p->shift += shift;
    w_m->change += shift/subtrees;
    w_p->prelim += shift;
    w_p->mod += shift;
}

void tree_layout_execute_shifts (layout_tree_node_t *v)
{
    double shift = 0;
    double change = 0;
    int ch_id;
    for (ch_id = v->num_children-1; ch_id >= 0; ch_id--) {
        layout_tree_node_t *w = v->children[ch_id];
        w->prelim += shift;
        w->mod += shift;
        change += w->change;
        shift += w->shift + change;
    }
}

layout_tree_node_t* tree_layout_ancestor (layout_tree_node_t *v_i_m, layout_tree_node_t *v,
                           layout_tree_node_t *default_ancestor)
{
    if (v_i_m->ancestor->parent == v->parent) {
        return v_i_m->ancestor;
    } else {
        return default_ancestor;
    }
}

void tree_layout_apportion (layout_tree_node_t *v, layout_tree_node_t **default_ancestor)
{
    layout_tree_node_t *v_i_p, *v_o_p, *v_i_m, *v_o_m;
    double s_i_p, s_o_p, s_i_m, s_o_m;
    if (v->child_id > 0) {
        v_i_p = v_o_p = v;
        v_i_m = left_sibling(v);
        v_o_m = leftmost(v_i_p->parent);
        s_i_p = v_i_p->mod;
        s_o_p = v_o_p->mod;
        s_i_m = v_i_m->mod;
        s_o_m = v_o_m->mod;
        while (tree_layout_next_right (v_i_m) != NULL && tree_layout_next_left (v_i_p) != NULL) {
            v_i_m = tree_layout_next_right (v_i_m);
            v_i_p = tree_layout_next_left (v_i_p);
            v_o_m = tree_layout_next_left (v_o_m);
            v_o_p = tree_layout_next_right (v_o_p);
            v_o_p->ancestor = v;
            double shift = (v_i_m->prelim + s_i_m) - (v_i_p->prelim + s_i_p) + v_i_m->width;
            if (shift > 0) {
                tree_layout_move_subtree (tree_layout_ancestor (v_i_m, v, *default_ancestor), v, shift);
                s_i_p += shift;
                s_o_p += shift;
            }
            s_i_m += v_i_m->mod;
            s_i_p += v_i_p->mod;
            s_o_m += v_o_m->mod;
            s_o_p += v_o_p->mod;
        }

        if (tree_layout_next_right (v_i_m) != NULL && tree_layout_next_right (v_o_p) == NULL) {
            v_o_p->thread = tree_layout_next_right (v_i_m);
            v_o_p->mod += s_i_m - s_o_p;
        }

        if (tree_layout_next_left (v_i_p) != NULL && tree_layout_next_left (v_o_m) == NULL) {
            v_o_m->thread = tree_layout_next_left (v_i_p);
            v_o_m->mod += s_i_p - s_o_m;
            *default_ancestor = v;
        }
    }
}

void tree_layout_first_walk (layout_tree_node_t *v)
{
    if (v->num_children == 0) {
        v->prelim = 0;
        if (v->child_id > 0) {
            layout_tree_node_t *w = left_sibling(v);
            v->prelim = w->prelim + w->width;
        }
    } else {
        layout_tree_node_t *default_ancestor = v->children[0];
        int ch_id;
        for (ch_id=0; ch_id<v->num_children; ch_id++) {
            layout_tree_node_t *w = v->children[ch_id];
            tree_layout_first_walk (w);
            tree_layout_apportion (w, &default_ancestor);
        }

        tree_layout_execute_shifts (v);
        double midpoint = (leftmost(v)->prelim + rightmost(v)->prelim)/2;
        if (v->child_id > 0) {
            layout_tree_node_t *w = left_sibling(v);
            v->prelim = w->prelim + w->width;
            v->mod = v->prelim - midpoint;
        } else {
            v->prelim = midpoint;
        }
    }
}

#define tree_layout_second_walk(r,v_sep,box) \
    tree_layout_second_walk_helper(r,v_sep,box,-r->prelim,0)
void tree_layout_second_walk_helper (layout_tree_node_t *v,
                                     double v_separation,
                                     box_t *box,
                                     double m, double l)
{
    v->pos = VECT2(v->prelim + m, l*(v_separation));
    if (box != NULL) {
        box->min.x = MIN(box->min.x, v->pos.x);
        box->min.y = MIN(box->min.y, v->pos.y);
        box->max.x = MAX(box->max.x, v->pos.x);
        box->max.y = MAX(box->max.y, v->pos.y);
    }

    int ch_id;
    for (ch_id = 0; ch_id<v->num_children; ch_id++) {
        layout_tree_node_t *w = v->children[ch_id];
        tree_layout_second_walk_helper (w, v_separation, box, m+v->mod, l+1);
    }
}

#define layout_tree_preorder_print(r) layout_tree_preorder_print_helper(r,0)
void layout_tree_preorder_print_helper (layout_tree_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] x: %f, y: %f\n", v->node_id, v->pos.x, v->pos.y);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        layout_tree_preorder_print_helper (v->children[ch_id], l+1);
    }
}

#define backtrack_tree_preorder_print(r) backtrack_tree_preorder_print_helper(r,0)
void backtrack_tree_preorder_print_helper (struct backtrack_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] : %d\n", v->id, v->val);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        backtrack_tree_preorder_print_helper (v->children[ch_id], l+1);
    }
}

#define draw_view_tree_preorder(cr,n,x,x_sc,y,node_r,line_widths) \
    draw_view_tree_preorder_helper(cr,n,x,x_sc,y,node_r,line_widths,NULL,0)
void draw_view_tree_preorder_helper (cairo_t *cr, layout_tree_node_t *n,
                                     double x, double x_scale, double y,
                                     double node_r, double *line_widths,
                                     layout_tree_node_t *parent, int l)
{
    if (node_r != 0) {
        cairo_arc (cr, x+n->pos.x*x_scale, y+n->pos.y, node_r, 0, 2*M_PI);
        cairo_fill (cr);
    }

    if (parent != NULL) {
        if (line_widths != NULL) {
            cairo_set_line_width (cr, line_widths[l]);
        }
        cairo_move_to (cr, x+parent->pos.x*x_scale, y+parent->pos.y);
        cairo_line_to (cr, x+n->pos.x*x_scale, y+n->pos.y);
        cairo_stroke (cr);
    }

    int ch_id;
    for (ch_id=0; ch_id<n->num_children; ch_id++) {
        draw_view_tree_preorder_helper (cr, n->children[ch_id], x, x_scale, y,
                                        node_r, line_widths, n, l+1);
    }
}

void seq_tree_draw (char* fname, struct sequence_store_t *stor, double width,
                    double ar, double line_width, double min_line_width, double node_r)
{
    mem_pool_t pool = {0};
    if (stor->tree_root == NULL) {
        seq_tree_end (stor);
        if (stor->tree_root == NULL) {
            printf ("Do not use SEQ_DRY_RUN to draw tree. Aborting.\n");
            return;
        }
    }

    char *png_fname = NULL, *pdf_fname = NULL;
    char *ext = get_extension (fname);
    if (ext == NULL) {
        pdf_fname = add_extension (&pool, fname, "pdf");
        png_fname = add_extension (&pool, fname, "png");
    } else {
        if (strncmp (ext, "png", 3) == 0) {
            png_fname = fname;
        } else if (strncmp (ext, "pdf", 3) == 0) {
            pdf_fname = fname;
        } else {
            printf ("Invalid file format, only pdf and png supported.\n");
            return;
        }
    }

    // Compute line width
    //  If min_line_width != 0, then we use it to stroke lines with a width
    //  according to the number of nodes in the level.
    //  Otherwies, all lines are stroked with line_with.
    if (min_line_width == 0) {
        min_line_width = line_width;
    }
    uint64_t *nodes_per_l = get_nodes_per_len (stor->tree_root, &pool, stor->final_max_len);
    double *line_widths = mem_pool_push_size (&pool, sizeof(double)*(stor->final_max_len+1));
    uint64_t min_num_nodes = UINT64_MAX; //Minumum number of nodes in a level excluding the root and las one.
    uint64_t max_num_nodes = 0;
    int i;
    for (i=1; i<stor->final_max_len; i++) {
        min_num_nodes = MIN(min_num_nodes, nodes_per_l[i]);
        max_num_nodes = MAX(max_num_nodes, nodes_per_l[i]);
    }

    double fact = ((line_width-min_line_width)/(double)(max_num_nodes-min_num_nodes));
    for (i=1; i<=stor->final_max_len; i++) {
        line_widths[i] = min_line_width+fact*(max_num_nodes - nodes_per_l[i]);
    }

    // Compute the actual tree layout
    double heigh = width/ar;
    double h_margin = width*0.05;
    double v_margin = heigh*0.05;
    double h_separation = 1, v_separation = (heigh-v_margin)/stor->final_max_len;

    box_t bnd_box;
    BOX_X_Y_W_H(bnd_box,0,0,0,0);

    layout_tree_node_t *root = create_layout_tree (&pool, h_separation, stor->tree_root);

    tree_layout_first_walk (root);
    tree_layout_second_walk (root, v_separation, &bnd_box);

    cairo_surface_t *surface;

    if (pdf_fname != NULL) {
        surface = cairo_pdf_surface_create (pdf_fname, width, heigh);
    } else {
        surface = cairo_pdf_surface_create_for_stream (NULL, NULL, width, heigh);
    }

    cairo_t *cr = cairo_create (surface);
    cairo_set_source_rgb (cr,1,1,1);
    cairo_paint (cr);
    cairo_set_source_rgb (cr,0,0,0);
    cairo_set_line_width (cr, line_width);

    //TODO: SHAME!! Should be using transforms instead.
    draw_view_tree_preorder (cr, root,
                             -bnd_box.min.x*(width-h_margin)/BOX_WIDTH(bnd_box)
                             + h_margin/2, (width-h_margin)/BOX_WIDTH(bnd_box),
                             v_margin/2, node_r, line_widths);

    cairo_surface_flush (surface);

    if (png_fname != NULL) {
        cairo_status_t retval = cairo_surface_write_to_png (surface, png_fname);
        if (retval != CAIRO_STATUS_SUCCESS) {
            printf ("Error: %s\n", cairo_status_to_string (retval));
        }
    }

    cairo_surface_destroy (surface);
    cairo_destroy (cr);
    mem_pool_destroy (&pool);
}
#endif /*CAIRO_PDF_H*/

#endif /*SEQUENCE_STORE_IMPL*/
