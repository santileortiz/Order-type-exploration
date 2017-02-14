#if !defined(OT_DB_H)
#include "geometry_combinatorics.h"
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>

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

int open_database (int n)
{
    if (__g_db_data.db) {
        close(__g_db_data.db);
    }
    __g_db_data.n = n;
    __g_db_data.indx = -1;
    __g_db_data.eof_reached = 0;

    if (n>8 && n<11) {
        __g_db_data.coord_size = 16;
    } else if (n>2) {
        __g_db_data.coord_size = 8;
    } else {
        printf ("Could not open order type database. Invalid n.\n");
        return 0;
    }

    __g_db_data.ot_size = 2*n*__g_db_data.coord_size/8;

    switch (n) {
        case 11:
            __g_db_data.num_order_types = 2334512907;
            break;
        case 10:
            __g_db_data.num_order_types = 14309547;
            break;
        case 9:
            __g_db_data.num_order_types = 158817;
            break;
        case 8:
            __g_db_data.num_order_types = 3315;
            break;
        case 7:
            __g_db_data.num_order_types = 135;
            break;
        case 6:
            __g_db_data.num_order_types = 16;
            break;
        case 5:
            __g_db_data.num_order_types = 3;
            break;
        case 4:
            __g_db_data.num_order_types = 2;
            break;
        case 3:
            __g_db_data.num_order_types = 1;
            break;
    }

    char filename[13];
    sprintf (filename, "otypes%02d.b%02u", n, __g_db_data.coord_size);
    __g_db_data.db = open (filename, O_RDONLY);
    if (__g_db_data.db == -1) {
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
