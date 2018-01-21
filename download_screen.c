/*
 * Copiright (C) 2018 Santiago León O.
 */

struct database_download_t {
    mem_pool_t mem;
    bool screen_built;
    bool redraw;
    pthread_t thread;
    volatile int lock;
    volatile bool *downloading;
    bool success;
    int curr_download;
    double percentage_completed;
    int num_missing;
    int missing[10];
    char progress_str[40];
    volatile int writing_str;
};

DOWNLOAD_PROGRESS_CALLBACK(db_dl_progress)
{
    struct database_download_t *dl_st = clientp;

    start_mutex (&dl_st->lock);
    if (dltotal == 0) {
        dl_st->percentage_completed = 0;
    } else {
        dl_st->percentage_completed = (double)dlnow*100/(double)dltotal;
    }
    end_mutex (&dl_st->lock);

    dl_st->redraw = true;
    return 0;
}

void* download_thread (void *arg)
{
    struct database_download_t *dl_st = (struct database_download_t*) arg;
    char *dir_path = sh_expand (APPLICATION_DATA, NULL);

    char *full_path = mem_pool_push_size (&dl_st->mem, strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);

    char *url = mem_pool_push_size(&dl_st->mem, strlen(OTDB_URL)+strlen(otdb_names[10])+1);
    char *u_loc = stpcpy (url, OTDB_URL);

    int i;
    for (i=0; i<dl_st->num_missing; i++) {
        strcpy (f_loc, otdb_names[dl_st->missing[i]]);
        strcpy (u_loc, otdb_names[dl_st->missing[i]]);

        dl_st->curr_download = i+1;
        dl_st->percentage_completed = 0;

        dl_st->success = download_file_cbk (url, full_path, db_dl_progress, dl_st);
        if (!dl_st->success) {
            break;
        }
    }
    *dl_st->downloading = false;
    free (dir_path);
    mem_pool_destroy (&dl_st->mem);
    pthread_exit (0);
}

bool download_database_screen (struct app_state_t *st, app_graphics_t *graphics)
{
    struct database_download_t *dl_st = st->dl_st;
    if (!dl_st->screen_built) {

        label_centered ("Downloading database", graphics->width/2, graphics->height/2 - 10,
               st);

        sprintf (dl_st->progress_str, " 1 of %d: %s (  0.0%%) ",
                 dl_st->num_missing, otdb_names[dl_st->missing[0]]);
        label_centered (dl_st->progress_str, graphics->width/2, graphics->height/2 + 10,
               st);

        dl_st->percentage_completed = 0;
        dl_st->downloading = &st->download_database;
        dl_st->screen_built = true;
        dl_st->redraw = true;
        dl_st->lock = 0;
        pthread_create (&dl_st->thread, NULL, download_thread, dl_st);
    }

    if (dl_st->redraw) {
        cairo_t *cr = graphics->cr;
        cairo_clear (cr);
        cairo_set_source_rgb (cr, bg_color.r, bg_color.g, bg_color.b);
        cairo_paint (cr);

        start_mutex (&dl_st->lock);
        sprintf (dl_st->progress_str, "%d of %d: %s (%.1f%%)",
                 dl_st->curr_download, dl_st->num_missing,
                 otdb_names[dl_st->missing[dl_st->curr_download-1]],
                 dl_st->percentage_completed);
        end_mutex (&dl_st->lock);

        int i;
        for (i=0; i<st->num_layout_boxes; i++) {
            struct css_box_t *style = st->layout_boxes[i].style;
            layout_box_t *layout = &st->layout_boxes[i];
            if (layout->style != NULL) {
                css_box_draw (graphics, style, layout);
            } else {
                invalid_code_path;
            }

        }
    }

    return dl_st->redraw;
}

