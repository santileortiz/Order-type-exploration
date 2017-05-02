#include "tree_mode.h"
#include "common.h"

typedef enum {VTN_OPEN, VTN_CLOSED, VTN_IGNORED} vtn_state_t;
typedef enum {VTN_FULL, VTN_COMPACT} vtn_view_type_t;

struct _view_tree_node_t {
    vtn_state_t state;
    vtn_view_type_t view_type;
    uint32_t node_id;
    uint32_t val;
    box_t box;

    uint32_t num_children;
    struct _view_tree_node_t *children[1];
};
typedef struct _view_tree_node_t view_tree_node_t;

#define push_view_node(pool, num_children) (num_children)>1? \
                       mem_pool_push_size((pool), sizeof(view_tree_node_t) + \
                       (num_children-1)*sizeof(view_tree_node_t*)) : \
                       mem_pool_push_size((pool), sizeof(view_tree_node_t))
view_tree_node_t* create_view_tree (mem_pool_t *pool, backtrack_node_t *node)
{
    view_tree_node_t *view_node = push_view_node (pool, node->num_children);
    *view_node = (view_tree_node_t){0};
    view_node->node_id = node->id;
    view_node->val = node->val;
    view_node->num_children = node->num_children;

    int ch_id;
    for (ch_id=0; ch_id<node->num_children; ch_id++) {
        view_node->children[ch_id] =
            create_view_tree (pool, node->children[ch_id]);
    }
    return view_node;
}

int view_tree_ignore (view_tree_node_t *view_node, int depth, int l)
{
    if (view_node->num_children == 0) {
        if (l < depth) {
            view_node->state = VTN_IGNORED;
        }
        return 1;
    }

    int ch_id, h = 0;
    for (ch_id=0; ch_id<view_node->num_children; ch_id++) {
        h = MAX(h, view_tree_ignore (view_node->children[ch_id], depth, l+1));
    }

    if (h+l != depth) {
        view_node->state = VTN_IGNORED;
    }

    return h+1;
}

#define view_tree_print(r) view_tree_print_helper(r,0)
void view_tree_print_helper (view_tree_node_t *v, int l)
{
    int i = l;
    while (i>0) {
        printf (" ");
        i--;
    }
    printf ("[%d] s: %d, val: %d\n", v->node_id, v->state, v->val);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        view_tree_print_helper (v->children[ch_id], l+1);
    }
}

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
    box_t *box;
    uint32_t child_id; // position among its siblings
    uint32_t num_children;
    struct _layout_tree_node_t *children[1];
};
typedef struct _layout_tree_node_t layout_tree_node_t;

#define push_layout_node(buff, num_children) (num_children)>1? \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t) + \
                        (num_children-1)*sizeof(layout_tree_node_t*)) : \
                         mem_pool_push_size((buff), sizeof(layout_tree_node_t))

#define create_layout_tree(pool,full_width,compact_width,sibling_sep,node) \
    create_layout_tree_helper(pool, full_width, compact_width, sibling_sep, node, NULL, 0)
layout_tree_node_t* create_layout_tree_helper (mem_pool_t *pool,
                                               double full_width, double compact_width,
                                               double sibling_separation,
                                               view_tree_node_t *node,
                                               layout_tree_node_t *parent, uint32_t child_id)
{
    layout_tree_node_t *lay_node = push_layout_node (pool, node->num_children);
    *lay_node = (layout_tree_node_t){0};
    lay_node->box = &node->box;
    lay_node->ancestor = lay_node;
    lay_node->parent = parent;
    lay_node->child_id = child_id;
    lay_node->node_id = node->node_id;

    if (node->view_type == VTN_COMPACT) {
        lay_node->width = compact_width + sibling_separation;
    } else if (node->view_type == VTN_FULL) {
        lay_node->width = full_width + sibling_separation;
    }

    view_tree_node_t *not_ignored[node->num_children];
    int ch_id, num_not_ignored = 0;
    for (ch_id=0; ch_id<node->num_children; ch_id++) {
        if (node->children[ch_id]->state != VTN_IGNORED) {
            not_ignored[num_not_ignored] = node->children[ch_id];
            num_not_ignored++;
        }
    }

    if (node->state == VTN_OPEN) {
        lay_node->num_children = num_not_ignored;
        for (ch_id=0; ch_id<num_not_ignored; ch_id++) {
            lay_node->children[ch_id] =
                create_layout_tree_helper (pool, full_width, compact_width,
                                           sibling_separation, not_ignored[ch_id], lay_node, ch_id);
        }
    } else {
        lay_node->num_children = 0;
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
            v_o_m->thread = tree_layout_next_left (v_o_m);
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
        int midpoint = (leftmost(v)->prelim + rightmost(v)->prelim)/2;
        if (v->child_id > 0) {
            layout_tree_node_t *w = left_sibling(v);
            v->prelim = w->prelim + w->width;
            v->mod = v->prelim - midpoint;
        } else {
            v->prelim = midpoint;
        }
    }
}

#define tree_layout_second_walk(r,size,margins) \
    tree_layout_second_walk_helper(r,size,margins,-r->prelim,0)
void tree_layout_second_walk_helper (layout_tree_node_t *v,
                                     vect2_t size, vect2_t margins,
                                     double m, double l)
{
    BOX_X_Y_W_H(*(v->box), v->prelim + m, l*(size.y+margins.y), v->width-margins.x, size.y);
    int ch_id;
    for (ch_id = 0; ch_id<v->num_children; ch_id++) {
        layout_tree_node_t *w = v->children[ch_id];
        tree_layout_second_walk_helper (w, size, margins, m+v->mod, l+1);
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
    printf ("[%d] x: %f, y: %f\n", v->node_id, v->box->min.x, v->box->min.y);

    int ch_id;
    for (ch_id=0; ch_id<v->num_children; ch_id++) {
        layout_tree_preorder_print_helper (v->children[ch_id], l+1);
    }
}

#define backtrack_tree_preorder_print(r) backtrack_tree_preorder_print_helper(r,0)
void backtrack_tree_preorder_print_helper (backtrack_node_t *v, int l)
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

void draw_permutation (cairo_t *cr, uint64_t perm_id, box_t *dest, struct tree_mode_state_t *tree_mode)
{
    int e[2*tree_mode->n];
    edges_from_permutation (tree_mode->all_perms, perm_id, tree_mode->n, e);

    box_t l_dest = *dest;
    l_dest.min.x += tree_mode->point_radius;
    l_dest.min.y += tree_mode->point_radius;
    l_dest.max.x -= tree_mode->point_radius;
    l_dest.max.y -= tree_mode->point_radius;

    transf_t graph_to_dest;
    box_t *src = &tree_mode->points_bb;
    compute_best_fit_box_to_box_transform (&graph_to_dest, src, &l_dest);

    cairo_set_source_rgb (cr,0,0,0);
    int i;
    for (i=0; i<2*tree_mode->n; i++) {
        vect2_t p = tree_mode->points[i];
        apply_transform (&graph_to_dest, &p);
        cairo_arc (cr, p.x, p.y, tree_mode->point_radius, 0, 2*M_PI);
        cairo_fill (cr);
    }

    int line_width = 2;
    cairo_set_line_width (cr, line_width);
    for (i=0; i<tree_mode->n; i++) {
        vect2_t p1 = tree_mode->points[e[2*i]];
        apply_transform (&graph_to_dest, &p1);
        pixel_align_as_line (&p1, line_width);

        vect2_t p2 = tree_mode->points[e[2*i+1]];
        apply_transform (&graph_to_dest, &p2);
        pixel_align_as_line (&p2, line_width);

        cairo_move_to (cr, p1.x, p1.y);
        cairo_line_to (cr, p2.x, p2.y);
        cairo_stroke (cr);
    }
}

void draw_view_tree_preorder (cairo_t *cr, view_tree_node_t *v, double x, double y,
                              struct tree_mode_state_t *tree_mode)
{
    box_t dest_box;
    dest_box.min.x = v->box.min.x+x;
    dest_box.max.x = v->box.max.x+x;
    dest_box.min.y = v->box.min.y+y;
    dest_box.max.y = v->box.max.y+y;
    if (v->view_type == VTN_COMPACT || v->val == -1) {
        vect2_t p = VECT2 (dest_box.min.x + BOX_WIDTH(dest_box)/2, dest_box.min.y + BOX_HEIGHT(dest_box)/2);
        cairo_arc (cr, p.x, p.y, 7, 0, 2*M_PI);
        cairo_fill (cr);
    } else {
        draw_permutation (cr, v->val, &dest_box, tree_mode);
    }


    if (v->state == VTN_OPEN) {
        int ch_id;
        for (ch_id=0; ch_id<v->num_children; ch_id++) {
            if (v->children[ch_id]->state != VTN_IGNORED) {
                draw_view_tree_preorder (cr, v->children[ch_id], x, y, tree_mode);
            }
        }
    }
}

bool tree_mode (struct app_state_t *st, app_graphics_t *gr)
{
    struct tree_mode_state_t *tree_mode = st->tree_mode;
    cairo_t *cr = gr->cr;

    static view_tree_node_t *view_root;

    int n = 4;
    if (!st->tree_mode) {
        uint32_t mode_storage = megabyte(5);
        uint8_t *base = push_size (&st->memory, mode_storage);
        st->tree_mode = (struct tree_mode_state_t*)base;
        memory_stack_init (&st->tree_mode->memory,
                           mode_storage-sizeof(struct tree_mode_state_t),
                           base+sizeof(struct tree_mode_state_t));
        tree_mode = st->tree_mode;
        tree_mode->root_pos = VECT2 (100, 50);
        tree_mode->n = n;
        tree_mode->point_radius = 4;
        tree_mode->points = push_array (&tree_mode->memory, 2*tree_mode->n, vect2_t);
        bipartite_points (tree_mode->n, tree_mode->points, &tree_mode->points_bb, 1.618);

        tree_mode->all_perms = mem_pool_push_array (&tree_mode->pool, factorial(n)*n, int);
        compute_all_permutations (n, tree_mode->all_perms);

        mem_pool_t bt_res_pool = {0};
        backtrack_node_t *bt_root = matching_decompositions_over_complete_bipartite_graphs (n, &bt_res_pool,
                                                                &tree_mode->num_nodes,
                                                                tree_mode->all_perms);
        view_root = create_view_tree (&tree_mode->pool, bt_root);
        view_tree_ignore (view_root, n, 0);
        //view_tree_print (view_root);
        mem_pool_destroy (&bt_res_pool);

        int dest_width = 50;
        int dest_height = (dest_width-2*tree_mode->point_radius)/BOX_AR(tree_mode->points_bb)+
            2*tree_mode->point_radius;

        vect2_t dest_size = VECT2 (dest_width, dest_height);
        vect2_t margins = VECT2 (30, 20);
        double compact_width = 14;

        int ch_id;
        for (ch_id=1; ch_id<view_root->num_children; ch_id++) {
            view_tree_node_t *w = view_root->children[ch_id];
            if (w->state != VTN_IGNORED) {
                w->view_type = VTN_COMPACT;
                w->state = VTN_CLOSED;
            }
        }

        mem_pool_t layout_tree = {0};
        layout_tree_node_t *root = create_layout_tree (&layout_tree, dest_size.x,
                                                       compact_width, margins.x, view_root);
        root->width = compact_width + margins.x;

        tree_layout_first_walk (root);
        tree_layout_second_walk (root, dest_size, margins);
        //layout_tree_preorder_print (root);
        //printf ("\n");
        mem_pool_destroy (&layout_tree);
    }

    if (st->dragging[0]) {
        tree_mode->root_pos.x += st->ptr_delta.x;
        tree_mode->root_pos.y += st->ptr_delta.y;
        st->input.force_redraw = true;
    }

    if (st->input.force_redraw) {
        cairo_set_source_rgb (cr, 1,1,1);
        cairo_paint (cr);
        cairo_set_source_rgb (cr,0,0,0);
        draw_view_tree_preorder (cr, view_root, tree_mode->root_pos.x, tree_mode->root_pos.y, tree_mode);

        return true;
    }
    return false;
}
