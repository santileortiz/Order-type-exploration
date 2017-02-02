//gcc -g -Wall -o bin/save_file_to_pdf save_file_to_pdf.c -lcairo -lm
#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>
#include <math.h>
#include "common.h"
#include "ot_db.h"
#include "geometry_combinatorics.h"

triangle_t triangle_for_indexes (order_type_t *ot, int *idx)
{
    triangle_t t;
    t.v[0] = ot->pts[idx[0]];
    t.v[1] = ot->pts[idx[1]];
    t.v[2] = ot->pts[idx[2]];
    return t;
}

typedef struct {
    double scale;
    double dx;
    double dy;
} transf_t;

void transform (point_t *p, transf_t *tr)
{
    p->x = tr->scale*(double)p->x+tr->dx;
    p->y = tr->scale*(255-(double)p->y)+tr->dy;
}

void draw_segment (cairo_t *cr, point_t p1, point_t p2, double line_width, transf_t *tr)
{
    cairo_set_line_width (cr, line_width);

    transform (&p1, tr);
    transform (&p2, tr);
    cairo_move_to (cr, p1.x, p1.y);
    cairo_line_to (cr, p2.x, p2.y);
    cairo_stroke (cr);
}

void draw_point (cairo_t *cr, double x, double y, transf_t *tr)
{
    point_t p;
    p.x = x;
    p.y = y;
    transform (&p, tr);
    cairo_arc (cr, p.x, p.y, 2, 0, 2*M_PI);
    cairo_fill (cr);
}


void draw_triangle (cairo_t *cr, triangle_t *t, transf_t *tr)
{
    cairo_new_path (cr);

    draw_segment (cr, t->v[0], t->v[1], 1, tr);
    draw_segment (cr, t->v[1], t->v[2], 1, tr);
    draw_segment (cr, t->v[2], t->v[0], 1, tr);
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

#if 0

#define PAGE_WIDTH 612
#define PAGE_HEIGHT 729

#define X_SLOTS 3
#define X_MARGIN 10 //pt
int main (void)
{
    FILE* file = fopen ("./cvx_all_thrackles_8.txt", "r");
    uint64_t th_id;

    cairo_surface_t *surface = cairo_pdf_surface_create ("n_8-ot_0-th_n.pdf", PAGE_WIDTH, PAGE_HEIGHT);
    cairo_t *cr = cairo_create (surface);

    int n = 8;
    int k = 8;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    open_database (n);
    order_type_t *ot = order_type_new (n, NULL);
    convex_ot (ot);

    double x_step = (PAGE_WIDTH-X_MARGIN)/X_SLOTS;
    double ot_side = x_step-X_MARGIN;
    double y_slots = (int)((PAGE_HEIGHT-X_MARGIN)/x_step);
    double y_margin = (PAGE_HEIGHT-ot_side*y_slots)/(y_slots+1);
    double y_step = ot_side+y_margin;
    transf_t transf;
    transf.scale = ot_side/255;
    transf.dx = X_MARGIN;
    transf.dy = y_margin;

    int x_slot=0, y_slot=0;
    while (EOF != fscanf (file, "%"SCNu64, &th_id)) {
        subset_it_seek (triangle_set_it, th_id);
        triangle_t t;
        int i;

        get_next_color (0);
        for (i=0; i<k; i++) {
            subset_it_seek (triangle_it, triangle_set_it->idx[i]);
            t = triangle_for_indexes (ot, triangle_it->idx);

            vect3_t color;
            get_next_color (&color);

            cairo_set_source_rgb (cr, color.r, color.g, color.b);
            draw_triangle (cr, &t, &transf);
        }

        cairo_set_source_rgb (cr, 0, 0, 0);
        for (i=0; i<n; i++) {
            draw_point (cr, ot->pts[i].x, ot->pts[i].y, &transf);
        }

        x_slot++;
        if (x_slot == X_SLOTS) {
            x_slot = 0;
            y_slot++;

            if ((y_slot) == y_slots) {
                cairo_show_page (cr);
                x_slot = 0;
                y_slot = 0;
            }
        }
        transf.dx = X_MARGIN+x_slot*x_step;
        transf.dy = y_margin+y_slot*y_step;

    }

    cairo_surface_flush (surface);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);
}

#else
#define PAGE_WIDTH 612
#define PAGE_HEIGHT 729

#define X_SLOTS 3
#define X_MARGIN 10 //pt
int main (void)
{
    FILE* file = fopen ("./thrackle_for_each_8.txt", "r");
    uint64_t th_id;

    cairo_surface_t *surface = cairo_pdf_surface_create ("n_8-ot_n-th_1.pdf", PAGE_WIDTH, PAGE_HEIGHT);
    cairo_t *cr = cairo_create (surface);

    int n = 8;
    int k = 8;

    subset_it_t *triangle_it = subset_it_new (n, 3, NULL);
    subset_it_t *triangle_set_it = subset_it_new (triangle_it->size, k, NULL);

    subset_it_precompute (triangle_it);
    subset_it_precompute (triangle_set_it);

    open_database (n);
    order_type_t *ot = order_type_new (n, NULL);

    double x_step = (PAGE_WIDTH-X_MARGIN)/X_SLOTS;
    double ot_side = x_step-X_MARGIN;
    double y_slots = (int)((PAGE_HEIGHT-X_MARGIN)/x_step);
    double y_margin = (PAGE_HEIGHT-ot_side*y_slots)/(y_slots+1);
    double y_step = ot_side+y_margin;
    transf_t transf;
    transf.scale = ot_side/255;
    transf.dx = X_MARGIN;
    transf.dy = y_margin;

    int x_slot=0, y_slot=0;
    while (EOF != fscanf (file, "%"SCNu64, &th_id)) {
        db_next (ot);
        subset_it_seek (triangle_set_it, th_id);
        triangle_t t;
        int i;

        get_next_color (0);
        for (i=0; i<k; i++) {
            subset_it_seek (triangle_it, triangle_set_it->idx[i]);
            t = triangle_for_indexes (ot, triangle_it->idx);

            vect3_t color;
            get_next_color (&color);

            cairo_set_source_rgb (cr, color.r, color.g, color.b);
            draw_triangle (cr, &t, &transf);
        }

        cairo_set_source_rgb (cr, 0, 0, 0);
        for (i=0; i<n; i++) {
            draw_point (cr, ot->pts[i].x, ot->pts[i].y, &transf);
        }

        x_slot++;
        if (x_slot == X_SLOTS) {
            x_slot = 0;
            y_slot++;

            if ((y_slot) == y_slots) {
                cairo_show_page (cr);
                x_slot = 0;
                y_slot = 0;
            }
        }
        transf.dx = X_MARGIN+x_slot*x_step;
        transf.dy = y_margin+y_slot*y_step;
    }

    cairo_surface_flush (surface);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);
}
#endif
