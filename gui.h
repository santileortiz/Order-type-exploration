/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(GUI_H)

#define RGBA VECT4
#define RGB(r,g,b) VECT4(r,g,b,1)

void cairo_clear (cairo_t *cr)
{
    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);
}

int _g_gui_num_colors = 0;
vect4_t _g_gui_colors[50];

typedef struct {
    double scale_x;
    double scale_y;
    double dx;
    double dy;
} transf_t;

void apply_transform (transf_t *tr, vect2_t *p)
{
    p->x = tr->scale_x*p->x + tr->dx;
    p->y = tr->scale_y*p->y + tr->dy;
}

void apply_transform_distance (transf_t *tr, vect2_t *p)
{
    p->x = tr->scale_x*p->x;
    p->y = tr->scale_y*p->y;
}

void apply_inverse_transform (transf_t *tr, vect2_t *p)
{
    p->x = (p->x - tr->dx)/tr->scale_x;
    p->y = (p->y - tr->dy)/tr->scale_y;
}

void apply_inverse_transform_distance (transf_t *tr, vect2_t *p)
{
    p->x = p->x/tr->scale_x;
    p->y = p->y/tr->scale_y;
}

// Calculates a ratio by which multiply box a so that it fits inside box b
double best_fit_ratio (double a_width, double a_height,
                       double b_width, double b_height)
{
    if (a_width/a_height < b_width/b_height) {
        return b_height/a_height;
    } else {
        return b_width/a_width;
    }
}

void compute_best_fit_box_to_box_transform (transf_t *tr, box_t *src, box_t *dest)
{
    double src_width = BOX_WIDTH(*src);
    double src_height = BOX_HEIGHT(*src);
    double dest_width = BOX_WIDTH(*dest);
    double dest_height = BOX_HEIGHT(*dest);

    tr->scale_x = best_fit_ratio (src_width, src_height,
                                dest_width, dest_height);
    tr->scale_y = tr->scale_x;
    tr->dx = dest->min.x + (dest_width-src_width*tr->scale_x)/2;
    tr->dy = dest->min.y + (dest_height-src_height*tr->scale_y)/2;
}

// TODO: Maybe in the future we want this to be defined for each different
// plattform we support. Maybe move _width_ and _height_ to app_input_t. Also
// _T_ does not belong here, it belongs to a "canvas" state, but we don't have
// that yet.
typedef struct {
    cairo_t *cr;
    PangoLayout *text_layout;
    uint16_t width;
    uint16_t height;
} app_graphics_t;

void pixel_align_as_line (vect2_t *p, int line_width)
{
    p->x = floor (p->x)+(double)(line_width%2)/2;
    p->y = floor (p->y)+(double)(line_width%2)/2;
}

void vect_floor (vect2_t *p)
{
    p->x = floor (p->x);
    p->y = floor (p->y);
}

void rounded_box_path (cairo_t *cr, double x, double y, double width, double height, double radius)
{
    cairo_move_to (cr, x, y+radius);
    cairo_arc (cr, x+radius, y+radius, radius, M_PI, 3*M_PI/2);
    cairo_arc (cr, x+width - radius, y+radius, radius, 3*M_PI/2, 0);
    cairo_arc (cr, x+width - radius, y+height - radius, radius, 0, M_PI/2);
    cairo_arc (cr, x+radius, y+height - radius, radius, M_PI/2, M_PI);
    cairo_close_path (cr);
}

bool is_point_in_box (double p_x, double p_y, double x, double y, double width, double height)
{
    if (p_x < x || p_x > x+width) {
        return false;
    } else if (p_y < y || p_y > y+height) {
        return false;
    } else {
        return true;
    }
}

bool is_box_visible (box_t *box, app_graphics_t *gr)
{
    return
        is_point_in_box (box->min.x, box->min.y, 0, 0, gr->width, gr->height) ||
        is_point_in_box (box->max.x, box->max.y, 0, 0, gr->width, gr->height) ||
        is_point_in_box (box->min.x, box->max.y, 0, 0, gr->width, gr->height) ||
        is_point_in_box (box->max.x, box->min.y, 0, 0, gr->width, gr->height);
}

typedef struct {
    char *s;
    double width;
    double height;

    // NOTE: This is a vector, that goes from the point where cairo was when the
    // text was drawn, to the top left corner of the logical rectangle (origin). We
    // compute position thinking about the origin, and in the end we subtract
    // pos.
    vect2_t pos;
} sized_string_t;

typedef enum {
    INACTIVE,
    PRESSED,
    RELEASED
} hit_status_t;

typedef enum {
    CSS_BUTTON,
    CSS_BUTTON_ACTIVE,
    CSS_BUTTON_SA,
    CSS_BUTTON_SA_ACTIVE,
    CSS_BACKGROUND,
    CSS_TEXT_ENTRY,
    CSS_TEXT_TEXT,
    CSS_TEXT_TEXT_SELECTED,
    CSS_TEXT_ENTRY_FOCUS,
    CSS_LABEL,
    CSS_TITLE_LABEL,

    CSS_NUM_STYLES
} css_style_t;

typedef enum {
    CSS_TEXT_ALIGN_INITIAL,
    CSS_TEXT_ALIGN_CENTER,
    CSS_TEXT_ALIGN_LEFT,
    CSS_TEXT_ALIGN_RIGHT
} css_text_align_t;

typedef enum {
    CSS_FONT_WEIGHT_NORMAL,
    CSS_FONT_WEIGHT_BOLD
} css_font_weight_t;

// NOTE: If more than 32 properties, change this into a #define
typedef enum {
    CSS_PROP_BORDER_RADIUS      = 1<<0,
    CSS_PROP_BORDER_COLOR       = 1<<1,
    CSS_PROP_BORDER_WIDTH       = 1<<2,
    CSS_PROP_PADDING_X          = 1<<3,
    CSS_PROP_PADDING_Y          = 1<<4,
    CSS_PROP_MIN_WIDTH          = 1<<5,
    CSS_PROP_MIN_HEIGHT         = 1<<6,
    CSS_PROP_BACKGROUND_COLOR   = 1<<7,
    CSS_PROP_COLOR              = 1<<8,
    CSS_PROP_TEXT_ALIGN         = 1<<9,
    CSS_PROP_FONT_WEIGHT        = 1<<10,
    CSS_PROP_BACKGROUND_IMAGE   = 1<<11 //Only gradients supported for now
} css_property_t;

struct css_box_t {
    css_property_t property_mask;
    struct css_box_t *selector_active;
    double border_radius;
    vect4_t border_color;
    double border_width;
    double padding_x;
    double padding_y;
    double min_width;
    double min_height;
    vect4_t background_color;
    vect4_t color;

    css_text_align_t text_align;
    css_font_weight_t font_weight;

    int num_gradient_stops;
    vect4_t *gradient_stops;
};

typedef enum {
    CSS_SEL_ACTIVE  = 1<<0
} css_selector_t;

typedef struct layout_box_t layout_box_t;
#define DRAW_CALLBACK(name) void name(app_graphics_t *gr, layout_box_t *layout)
typedef DRAW_CALLBACK(draw_callback_t);
struct layout_box_t {
    box_t box;
    css_selector_t selectors;
    bool status_changed;
    hit_status_t status;
    css_text_align_t text_align_override;
    struct css_box_t *style;
    draw_callback_t *draw;
    sized_string_t str;
};

struct selection_t {
    layout_box_t *dest;
    char *start;
    size_t len;
    vect4_t color;
    vect4_t background_color;
};

enum behavior_type_t {
    BEHAVIOR_BUTTON,
    BEHAVIOR_TEXT_ENTRY
};

struct behavior_t {
    enum behavior_type_t type;
    layout_box_t *box;
    struct behavior_t *next;
};

struct gui_state_t {
    mem_pool_t pool;
    struct selection_t selection;
    struct behavior_t *behaviors;
};

void add_behavior (struct gui_state_t *gui_st, layout_box_t *box, enum behavior_type_t type)
{
    printf ("Adding behavior\n");
    struct behavior_t *new_behavior =
        mem_pool_push_size_full (&gui_st->pool, sizeof(struct behavior_t),
                                 POOL_ZERO_INIT);
    new_behavior->next = gui_st->behaviors;
    gui_st->behaviors = new_behavior;
    new_behavior->type = type;
    new_behavior->box = box;
}

struct gui_state_t *global_gui_st;

void update_font_description (struct css_box_t *box, PangoLayout *pango_layout)
{
    const PangoFontDescription *orig_font_desc = pango_layout_get_font_description (pango_layout);
    PangoFontDescription *new_font_desc = pango_font_description_copy (orig_font_desc);
    if (box->font_weight == CSS_FONT_WEIGHT_NORMAL) {
        pango_font_description_set_weight (new_font_desc, PANGO_WEIGHT_NORMAL);
    } else {
        pango_font_description_set_weight (new_font_desc, PANGO_WEIGHT_BOLD);
    }
    pango_layout_set_font_description (pango_layout, new_font_desc);
}

void sized_string_compute (sized_string_t *res, struct css_box_t *style, PangoLayout *pango_layout, char *str)
{
    // TODO: Try to refactor code so this fuction does not receive a PangoLayout
    // but creates it.
    update_font_description (style, pango_layout);

    // NOTE: I have the hunch that calls into Pango will be slow, so we ideally
    // would like to call this function just once for each string that needs to
    // be computed.
    // TODO: Time this function and check if it's really a problem.
    pango_layout_set_text (pango_layout, str, -1);

    PangoRectangle logical;
    pango_layout_get_pixel_extents (pango_layout, NULL, &logical);

    res->width = logical.width;
    res->height = logical.height;
    res->pos.x = logical.x;
    res->pos.y = logical.y;
}

// NOTE: len == 0/NULL means the string is null terminated. User should not try
// to print an empty string.
void render_text (cairo_t *cr, vect2_t pos, PangoLayout *pango_layout,
                  char *str, size_t len, vect4_t *color, vect4_t *bg_color,
                  vect2_t *out_pos)
{
    pango_layout_set_text (pango_layout, str, (len==0? -1 : len));

    PangoRectangle logical;
    pango_layout_get_pixel_extents (pango_layout, NULL, &logical);
    vect_floor (&pos);
    if (bg_color != NULL) {
        cairo_set_source_rgba (cr, bg_color->r, bg_color->g, bg_color->b,
                               bg_color->a);
        cairo_rectangle (cr, pos.x, pos.y, logical.width, logical.height);
        cairo_fill (cr);
    }

    if (out_pos != NULL ) {
        out_pos->x = pos.x + logical.width;
    }

    cairo_set_source_rgba (cr, color->r, color->g, color->b, color->a);
    cairo_move_to (cr, pos.x, pos.y);
    pango_cairo_show_layout (cr, pango_layout);
}

void layout_size_from_css_content_size (struct css_box_t *css_box,
                                        vect2_t *css_content_size, vect2_t *layout_size)
{
    layout_size->x = MAX(css_box->min_width, css_content_size->x)
                     + 2*(css_box->padding_x + css_box->border_width);
    layout_size->y = MAX(css_box->min_height, css_content_size->y)
                     + 2*(css_box->padding_y + css_box->border_width);
}

void css_box_draw (app_graphics_t *gr, struct css_box_t *box, layout_box_t *layout)
{
    cairo_t *cr = gr->cr;
    cairo_save (cr);
    cairo_translate (cr, layout->box.min.x, layout->box.min.y);

    // NOTE: This is the css content+padding.
    double content_width = BOX_WIDTH(layout->box) - 2*(box->border_width);
    double content_height = BOX_HEIGHT(layout->box) - 2*(box->border_width);

    rounded_box_path (cr, 0, 0,content_width+2*box->border_width,
                      content_height+2*box->border_width, box->border_radius);
    cairo_clip (cr);

    cairo_set_source_rgba (cr, box->background_color.r,
                          box->background_color.g,
                          box->background_color.b,
                          box->background_color.a);
    cairo_paint (cr);

    cairo_pattern_t *patt = cairo_pattern_create_linear (box->border_width, box->border_width,
                                                         0, content_height);

    if (box->num_gradient_stops > 0) {
        double position;
        double step = 1.0/((double)box->num_gradient_stops-1);
        int stop_idx = 0;
        for (position = 0.0; position<1; position+=step) {
            cairo_pattern_add_color_stop_rgba (patt, position, box->gradient_stops[stop_idx].r,
                                               box->gradient_stops[stop_idx].g,
                                               box->gradient_stops[stop_idx].b,
                                               box->gradient_stops[stop_idx].a);
            stop_idx++;
        }
        cairo_pattern_add_color_stop_rgba (patt, position, box->gradient_stops[stop_idx].r,
                                           box->gradient_stops[stop_idx].g,
                                           box->gradient_stops[stop_idx].b,
                                           box->gradient_stops[stop_idx].a);
        cairo_set_source (cr, patt);
        cairo_paint (cr);
    }

    rounded_box_path (cr, box->border_width/2, box->border_width/2,
                      content_width+box->border_width, content_height+box->border_width,
                      box->border_radius-box->border_width/2);
    cairo_set_source_rgba (cr, box->border_color.r,
                           box->border_color.g,
                           box->border_color.b,
                           box->border_color.a);
    cairo_set_line_width (cr, box->border_width);
    cairo_stroke (cr);

    if (layout->str.s != NULL) {

        PangoLayout *pango_layout = gr->text_layout;
        double text_pos_x = box->border_width, text_pos_y = box->border_width;
        sized_string_compute (&layout->str, box, pango_layout, layout->str.s);

        css_text_align_t effective_text_align;
        if (layout->text_align_override != CSS_TEXT_ALIGN_INITIAL) {
            effective_text_align = layout->text_align_override;
        } else {
            effective_text_align = box->text_align;
        }
        switch (effective_text_align) {
            case CSS_TEXT_ALIGN_LEFT:
                text_pos_x += -layout->str.pos.x;
                break;
            case CSS_TEXT_ALIGN_RIGHT:
                text_pos_x += content_width - layout->str.width - layout->str.pos.x;
                break;
            default: // CSS_TEXT_ALIGN_CENTER
                text_pos_x += (content_width - layout->str.width)/2 - layout->str.pos.x;
                break;
        }
        text_pos_y += (content_height - layout->str.height)/2 - layout->str.pos.y;

        update_font_description (box, pango_layout);

        struct selection_t *selection = &global_gui_st->selection;
        vect2_t pos = VECT2(text_pos_x, text_pos_y);
        if (selection->dest == layout) {
            if (selection->start != layout->str.s) {
                render_text (cr, pos, pango_layout, layout->str.s, selection->start - layout->str.s,
                             &box->color, NULL, &pos);
            }
            render_text (cr, pos, pango_layout, selection->start, selection->len,
                         &selection->color, &selection->background_color, &pos);
            render_text (cr, pos, pango_layout, selection->start+selection->len, 0,
                         &box->color, NULL, NULL);
        } else {
            render_text (cr, pos, pango_layout, layout->str.s, 0, &box->color, NULL, NULL);
        }

#if 0
        cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.3);
        cairo_rectangle (cr, box->border_width, box->border_width, content_width, content_height);
        cairo_fill (cr);

        cairo_set_source_rgba (cr, 0.0, 1.0, 0.0, 0.3);
        cairo_rectangle (cr, text_pos_x+layout->str.pos.x, text_pos_y+layout->str.pos.y,
                         layout->str.width, layout->str.height);
        cairo_fill (cr);
#endif
    }

    cairo_restore (cr);
}

void css_box_add_gradient_stops (struct css_box_t *box, int num_stops, vect4_t *stops)
{
    assert (_g_gui_num_colors+num_stops < ARRAY_SIZE(_g_gui_colors));
    box->num_gradient_stops = num_stops;
    box->gradient_stops = &_g_gui_colors[_g_gui_num_colors];

    int i = 0;
    while (i < num_stops) {
        _g_gui_colors[_g_gui_num_colors] = stops[i];
        _g_gui_num_colors++;
        i++;
    }
}

void rgba_to_hsla (vect4_t *rgba, vect4_t *hsla)
{
    double max = MAX(MAX(rgba->r,rgba->g),rgba->b);
    double min = MIN(MIN(rgba->r,rgba->g),rgba->b);

    // NOTE: This is the same for HSV
    if (max == min) {
        // NOTE: we define this, in reality H is undefined
        // in this case.
        hsla->h = INFINITY;
    } else if (max == rgba->r) {
        hsla->h = (60*(rgba->r-rgba->b)/(max-min) + 360);
        if (hsla->h > 360) {
            hsla->h -= 360;
        }
    } else if (max == rgba->g) {
        hsla->h = (60*(rgba->b-rgba->r)/(max-min) + 120);
    } else if (max == rgba->b) {
        hsla->h = (60*(rgba->r-rgba->g)/(max-min) + 240);
    } else {
        invalid_code_path;
    }
    hsla->h /= 360; // h\in[0,1]

    hsla->l = (max+min)/2;

    if (max == min) {
        hsla->s = 0;
    } else if (hsla->l <= 0.5) {
        hsla->s = (max-min)/(2*hsla->l);
    } else {
        hsla->s = (max-min)/(2-2*hsla->l);
    }

    hsla->a = rgba->a;
}

void hsla_to_rgba (vect4_t *hsla, vect4_t *rgba)
{
    double h_pr = hsla->h*6;
    double chroma = (1-fabs(2*hsla->l-1)) * hsla->s;
    // Alternative version can be:
    //
    //double chroma;
    //if (hsla->l <= 0.5) {
    //    chroma = (2*hsla->l)*hsla->s;
    //} else {
    //    chroma = (2-2*hsla->l)*hsla->s;
    //}

    double h_pr_mod_2 = ((int)h_pr)%2+(h_pr-(int)h_pr);
    double x = chroma*(1-fabs(h_pr_mod_2-1));
    double m = hsla->l - chroma/2;

    *rgba = VECT4(0,0,0,0);
    if (h_pr == INFINITY) {
        rgba->r = 0;
        rgba->g = 0;
        rgba->b = 0;
    } else if (h_pr < 1) {
        rgba->r = chroma;
        rgba->g = x;
        rgba->b = 0;
    } else if (h_pr < 2) {
        rgba->r = x;
        rgba->g = chroma;
        rgba->b = 0;
    } else if (h_pr < 3) {
        rgba->r = 0;
        rgba->g = chroma;
        rgba->b = x;
    } else if (h_pr < 4) {
        rgba->r = 0;
        rgba->g = x;
        rgba->b = chroma;
    } else if (h_pr < 5) {
        rgba->r = x;
        rgba->g = 0;
        rgba->b = chroma;
    } else if (h_pr < 6) {
        rgba->r = chroma;
        rgba->g = 0;
        rgba->b = x;
    }
    rgba->r += m;
    rgba->g += m;
    rgba->b += m;
    rgba->a = hsla->a;
}

vect4_t shade (vect4_t *in, double f)
{
    vect4_t temp;
    rgba_to_hsla (in, &temp);

    temp.l *= f;
    temp.l = temp.l>1? 1 : temp.l;
    temp.s *= f;
    temp.s = temp.s>1? 1 : temp.s;

    vect4_t ret;
    hsla_to_rgba (&temp, &ret);
    return ret;
}

void hsv_to_rgb (vect3_t *hsv, vect3_t *rgb)
{
    double h = hsv->E[0];
    double s = hsv->E[1];
    double v = hsv->E[2];
    assert (h <= 1 && s <= 1 && v <= 1);

    int h_i = (int)(h*6);
    double frac = h*6 - h_i;

    double m = v * (1 - s);
    double desc = v * (1 - frac*s);
    double asc = v * (1 - (1 - frac) * s);

    switch (h_i) {
        case 0:
            rgb->r = v;
            rgb->g = asc;
            rgb->b = m;
            break;
        case 1:
            rgb->r = desc;
            rgb->g = v;
            rgb->b = m;
            break;
        case 2:
            rgb->r = m;
            rgb->g = v;
            rgb->b = asc;
            break;
        case 3:
            rgb->r = m;
            rgb->g = desc;
            rgb->b = v;
            break;
        case 4:
            rgb->r = asc;
            rgb->g = m;
            rgb->b = v;
            break;
        case 5:
            rgb->r = v;
            rgb->g = m;
            rgb->b = desc;
            break;
    }
}

void rgba_print (vect4_t rgba)
{
    printf ("rgba");
    vect4_print(&rgba);
    printf ("\n");
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
        h += PHI_INV;
        if (h>1) {
            h -= 1;
        }
        hsv_to_rgb (&VECT3(h, 0.8, 0.7), color);
    }
}

vect4_t selected_bg_color = RGB(0.239216, 0.607843, 0.854902);
vect4_t selected_fg_color = RGB(1,1,1);

void init_button (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->min_height = 20;
    box->padding_x = 12;
    box->padding_y = 3;
    box->border_color = RGBA(0, 0, 0, 0.27);
    box->color = RGB(0.2, 0.2, 0.2);

    box->background_color = RGB(0.96, 0.96, 0.96);
    vect4_t stops[3] = {RGBA(0,0,0,0),
                        RGBA(0,0,0,0),
                        RGBA(0,0,0,0.04)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

void init_button_active (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->min_height = 20;
    box->padding_x = 12;
    box->padding_y = 3;
    box->border_color = RGBA(0, 0, 0, 0.27);
    box->color = RGB(0.2, 0.2, 0.2);

    box->background_color = RGBA(0, 0, 0, 0.05);
}

void init_suggested_action_button (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    init_button (box);
    box->border_color = shade (&selected_bg_color, 0.8);
    box->color = selected_fg_color;

    vect4_t stops[2] = {shade(&selected_bg_color,1.1),
                        shade(&selected_bg_color,0.9)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

void init_suggested_action_button_active (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    init_button (box);
    box->border_color = shade (&selected_bg_color, 0.8);
    box->color = selected_fg_color;


    vect4_t stops[2] = {shade(&selected_bg_color,1.05),
                        shade(&selected_bg_color,0.95)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);
}

void init_background (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    //box->padding_x = 12;
    //box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->border_color = RGBA(0, 0, 0, 0.27);
}

void init_text_entry (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 3;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->color = RGB(0.2, 0.2, 0.2);
    vect4_t stops[2] = {RGB(0.93,0.93,0.93),
                        RGB(0.97,0.97,0.97)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);

    box->border_color = RGBA(0, 0, 0, 0.25);
}

void init_text_entry_focused (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->border_radius = 2.5;
    box->border_width = 1;
    box->padding_x = 3;
    box->padding_y = 3;
    box->background_color = RGB(0.96, 0.96, 0.96);
    box->color = RGB(0.2, 0.2, 0.2);
    vect4_t stops[2] = {RGB(0.93,0.93,0.93),
                        RGB(0.97,0.97,0.97)};
    css_box_add_gradient_stops (box, ARRAY_SIZE(stops), stops);

    box->border_color = RGBA(0.239216, 0.607843, 0.854902, 0.8);
}

void init_label (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->background_color = RGBA(0, 0, 0, 0);
    box->color = RGB(0.2, 0.2, 0.2);
}

void init_title_label (struct css_box_t *box)
{
    *box = (struct css_box_t){0};
    box->background_color = RGBA(0, 0, 0, 0);
    box->color = RGBA(0.2, 0.2, 0.2, 0.7);
    box->font_weight = CSS_FONT_WEIGHT_BOLD;
}

#define GUI_H
#endif
