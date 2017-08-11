/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#if !defined(OT_DB_H)
#include "geometry_combinatorics.h"
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <curl/curl.h>

#include "slo_timers.h"

struct {
    int db;
    int n;
    int coord_size; //bits per coordinate in database
    int ot_size; //size of an order type in bytes (in the database)
    int eof_reached;
    uint64_t num_order_types;
    uint64_t indx;
    uint32_t buff[40];
} __g_db_data ;

int open_database (int n);
void db_next (order_type_t *ot);
void db_seek (order_type_t *ot, uint64_t id);
void db_prev (order_type_t *ot);
int db_is_eof ();

uint64_t db_num_order_types (int n)
{
    switch (n) {
        case 11:
            return 2334512907;
        case 10:
            return 14309547;
        case 9:
            return 158817;
        case 8:
            return 3315;
        case 7:
            return 135;
        case 6:
            return 16;
        case 5:
            return 3;
        case 4:
            return 2;
        case 3:
            return 1;
        default:
            return 0;
   }
}

int db_coord_size (int n)
{
    if (n>8 && n<11) {
        return 16;
    } else {
        return 8;
    }
}

char* otdb_names [] = { "", "", "",
                       "otypes03.b08", "otypes04.b08", "otypes05.b08", "otypes06.b08",
                       "otypes07.b08", "otypes08.b08", "otypes09.b16", "otypes10.b16"};


#define DEF_DB_LOCATION "~/.ot_viewer/"
#define OTDB_URL "http://www.ist.tugraz.at/aichholzer/research/rp/triangulations/ordertypes/data/"
void ensure_full_database ()
{
    char *dir_path = sh_expand (DEF_DB_LOCATION, NULL);
    struct stat st;

    int missing [10];
    int num_missing = 0;

    char *full_path = malloc (strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);
    int i;
    for (i=3; i<11; i++) {
        strcpy (f_loc, otdb_names[i]);
        if (stat(full_path, &st) == -1 && errno == ENOENT) {
            missing[num_missing] = i;
            num_missing++;
        }
    }

    if (num_missing > 0) {
        printf ("Missing %i order type databases.\n", num_missing);
        char *url = malloc(strlen(OTDB_URL)+strlen(otdb_names[10])+1);
        char *u_loc = stpcpy (url, OTDB_URL);

        CURL *h = curl_easy_init ();
        //curl_easy_setopt (h, CURLOPT_VERBOSE, 1);
        curl_easy_setopt (h, CURLOPT_WRITEFUNCTION, NULL); // Required on Windows
        for (i=0; i<num_missing; i++) {
            if (h) {
                strcpy (f_loc, otdb_names[missing[i]]);
                FILE *f = fopen (full_path, "w");
                curl_easy_setopt (h, CURLOPT_WRITEDATA, f);

                strcpy (u_loc, otdb_names[missing[i]]);
                printf ("Downloading: %s\n", url);
                curl_easy_setopt (h, CURLOPT_URL, url);
                curl_easy_perform (h);
                printf ("Downloaded: %s\n\n", full_path);
                fclose (f);
            }
        }
        curl_easy_cleanup (h);
        free (url);
    }
    free (dir_path);
    free (full_path);
}

int open_database (int n)
{
    if (__g_db_data.db) {
        close(__g_db_data.db);
    }
    __g_db_data.n = n;
    __g_db_data.indx = -1;
    __g_db_data.eof_reached = 0;
    __g_db_data.num_order_types = db_num_order_types(n);
    __g_db_data.coord_size = db_coord_size (n);
    __g_db_data.ot_size = 2*n*__g_db_data.coord_size/8;

    char *dir_path = sh_expand (DEF_DB_LOCATION, NULL);
    char *full_path = malloc (strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);
    strcpy (f_loc, otdb_names[n]);

    __g_db_data.db = open (full_path, O_RDONLY);
    free (full_path);
    if (__g_db_data.db == -1) {
        invalid_code_path;
        return 0;
    }

    return 1;
}

int read_ot_from_db (order_type_t *ot)
{
    int eof = 0;
    if (!read (__g_db_data.db, &__g_db_data.buff, __g_db_data.ot_size)) {
        return 0;
    }

    void *coord = __g_db_data.buff;
    if (__g_db_data.coord_size == 16) {
        int i;
        for (i=0; i<ot->n; i++) {
            ot->pts[i].x = ((uint16_t*)coord)[2*i];
            ot->pts[i].y = ((uint16_t*)coord)[2*i+1];
        }
    } else {
        int i;
        for (i=0; i<ot->n; i++) {
            ot->pts[i].x = ((uint8_t*)coord)[2*i];
            ot->pts[i].y = ((uint8_t*)coord)[2*i+1];
        }
    }
    return !eof;
}

void db_next (order_type_t *ot)
{
    assert (ot->n == __g_db_data.n);
    
    if (!read_ot_from_db (ot)) {
        __g_db_data.eof_reached = 1;
        lseek (__g_db_data.db, 0, SEEK_SET);
        read_ot_from_db (ot);
        __g_db_data.indx = 0;
    } else {
        __g_db_data.indx++;
        __g_db_data.eof_reached = 0;
    }
    ot->id = __g_db_data.indx;
}

int db_is_eof ()
{
    return __g_db_data.eof_reached;
}

void db_prev (order_type_t *ot)
{
    if (-1 == lseek (__g_db_data.db, -2*__g_db_data.ot_size, SEEK_CUR)) {
        if (errno == EINVAL) {
            lseek (__g_db_data.db, -__g_db_data.ot_size, SEEK_END);
            __g_db_data.indx = __g_db_data.num_order_types - 1;
        }
    } else {
        __g_db_data.indx--;
    }
    ot->id = __g_db_data.indx;
    read_ot_from_db (ot);
}

void db_seek (order_type_t *ot, uint64_t id)
{
    // TODO: This hasn't been tested for n>10, if we get the file for n=11,
    // this may throw EOVERFLOW, check it doesn't.
    if (id < __g_db_data.num_order_types) {
        __g_db_data.indx = id;
        ot->id = __g_db_data.indx;
        if (-1 == lseek (__g_db_data.db, id*__g_db_data.ot_size, SEEK_SET)) {
            if (errno == EOVERFLOW) {
                printf ("File offset too big\n");
            }
        }
        read_ot_from_db (ot);
    } else {
        printf ("There is no order type with that index\n");
    }
}

#define GEOMETRY_COMBINATORICS_H
#endif
