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
    int num_to_download;
    int to_download[10];
    char progress_str[40];
    volatile int writing_str;
    char* database_source;
    char* database_location;
};

void* download_thread (void *arg)
{
    struct database_download_t *dl_st = (struct database_download_t*) arg;
    char *dir_path = sh_expand (dl_st->database_location, NULL);

    char *full_path = mem_pool_push_size (&dl_st->mem, strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);

    char *url = mem_pool_push_size(&dl_st->mem, strlen(dl_st->database_source)+strlen(otdb_names[10])+1);
    char *u_loc = stpcpy (url, dl_st->database_source);

    bool download_error = false;
    int i;
    for (i=0; i<dl_st->num_to_download; i++) {
        strcpy (f_loc, otdb_names[dl_st->to_download[i]]);
        strcpy (u_loc, otdb_names[dl_st->to_download[i]]);

        dl_st->curr_download = i+1;
        dl_st->percentage_completed = 0;

        http_t* request = http_get (url, NULL);
        if (request) {
            http_status_t status = HTTP_STATUS_PENDING;
            while (status == HTTP_STATUS_PENDING) {
                status = http_process (request);

                if (request->content_length == 0) {
                    dl_st->percentage_completed = 0;
                } else {
                    dl_st->percentage_completed = (double)request->received_size*100/(double)request->content_length;
                }
            }

            if( status != HTTP_STATUS_FAILED ) {
                full_file_write (request->response_data, request->response_size, full_path);
            } else {
                printf( "HTTP request failed (%d): %s.\n", request->status_code, request->reason_phrase );
                download_error = true;
            }

            http_release( request );
        } else {
            download_error = true;
            break;
        }
    }

    if (download_error) {
        printf ("Errors occurred while downloading the database.\n");
    }


    *dl_st->downloading = false;
    // NOTE: We reset this to remove the layout boxes used for the download screen.
    global_gui_st->num_layout_boxes = 0;

    free (dir_path);
    mem_pool_destroy (&dl_st->mem);
    pthread_exit (0);
}

bool download_database_screen (struct app_state_t *st, app_graphics_t *graphics)
{
    struct database_download_t *dl_st = st->dl_st;
    if (!dl_st->screen_built) {

        dvec2 cent = DVEC2 (graphics->width/2, graphics->height/2 - 10);
        label ("Downloading database", POS_CENTERED(cent.x, cent.y-10));

        sprintf (dl_st->progress_str, " 1 of %d: %s (  0.0%%) ",
                 dl_st->num_to_download, otdb_names[dl_st->to_download[0]]);
        label (dl_st->progress_str, POS_CENTERED(cent.x, cent.y+10));

        dl_st->percentage_completed = 0;
        dl_st->downloading = &st->download_database;
        dl_st->screen_built = true;
        dl_st->redraw = true;
        dl_st->lock = 0;
        dl_st->database_source = st->config->database_source;
        dl_st->database_location = st->config->database_location;
        ensure_dir_exists_no_sh_expand (st->config->database_location);
        pthread_create (&dl_st->thread, NULL, download_thread, dl_st);
    }

    if (dl_st->redraw) {
        cairo_t *cr = graphics->cr;
        cairo_clear (cr);
        cairo_set_source_rgb (cr, bg_color.r, bg_color.g, bg_color.b);
        cairo_paint (cr);

        start_mutex (&dl_st->lock);
        sprintf (dl_st->progress_str, "%d of %d: %s (%.1f%%)",
                 dl_st->curr_download, dl_st->num_to_download,
                 otdb_names[dl_st->to_download[dl_st->curr_download-1]],
                 dl_st->percentage_completed);
        end_mutex (&dl_st->lock);

        int i;
        for (i=0; i<st->gui_st.num_layout_boxes; i++) {
            struct css_box_t *style = st->gui_st.layout_boxes[i].style;
            layout_box_t *layout = &st->gui_st.layout_boxes[i];
            if (layout->style != NULL) {
                css_box_draw (graphics, style, layout);
            } else {
                invalid_code_path;
            }

        }
    }

    return dl_st->redraw;
}

