/* 
 * Copiright (C) 2017 Santiago León O.
 */

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
                                               backtrack_node_t *node,
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

#define draw_view_tree_preorder(cr,n,x,x_sc,y,node_r) \
    draw_view_tree_preorder_helper(cr,n,x,x_sc,y,node_r,NULL)
void draw_view_tree_preorder_helper (cairo_t *cr, layout_tree_node_t *n, double
                                     x, double x_scale, double y, double node_r,
                                     layout_tree_node_t *parent)
{
    if (node_r != 0) {
        cairo_arc (cr, x+n->pos.x*x_scale, y+n->pos.y, node_r, 0, 2*M_PI);
        cairo_fill (cr);
    }

    if (parent != NULL) {
        cairo_move_to (cr, x+parent->pos.x*x_scale, y+parent->pos.y);
        cairo_line_to (cr, x+n->pos.x*x_scale, y+n->pos.y);
        cairo_stroke (cr);
    }

    int ch_id;
    for (ch_id=0; ch_id<n->num_children; ch_id++) {
        draw_view_tree_preorder_helper (cr, n->children[ch_id], x, x_scale, y, node_r, n);
    }
}

