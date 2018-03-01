/*
 * Copiright (C) 2017 Santiago León O.
 */

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

void check_database (int missing[10], int *num_missing)
{
    char *dir_path = sh_expand (APPLICATION_DATA, NULL);
    struct stat st;

    *num_missing = 0;

    char *full_path = malloc (strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);
    int i;
    for (i=3; i<11; i++) {
        strcpy (f_loc, otdb_names[i]);
        if (stat(full_path, &st) == -1 && errno == ENOENT) {
            missing[*num_missing] = i;
            (*num_missing)++;
        }
    }
    free (full_path);
    free (dir_path);
}

#ifdef __CURL_CURL_H
#define OTDB_URL "http://www.ist.tugraz.at/aichholzer/research/rp/triangulations/ordertypes/data/"
void ensure_full_database ()
{
    int missing [10];
    int num_missing = 0;
    check_database (missing, &num_missing);

    if (num_missing > 0) {
        printf ("Missing %i order type databases.\n", num_missing);

        char *dir_path = sh_expand (APPLICATION_DATA, NULL);

        char *full_path = malloc (strlen(dir_path)+strlen(otdb_names[10])+1);
        char *f_loc = stpcpy (full_path, dir_path);

        char *url = malloc(strlen(OTDB_URL)+strlen(otdb_names[10])+1);
        char *u_loc = stpcpy (url, OTDB_URL);

        int i;
        for (i=0; i<num_missing; i++) {
            strcpy (f_loc, otdb_names[missing[i]]);
            strcpy (u_loc, otdb_names[missing[i]]);
            download_file (url, full_path);
        }

        free (url);
        free (full_path);
        free (dir_path);
    }
}
#endif /*__CURL_CURL_H*/

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

    char *dir_path = sh_expand (APPLICATION_DATA, NULL);
    char *full_path = malloc (strlen(dir_path)+strlen(otdb_names[10])+1);
    char *f_loc = stpcpy (full_path, dir_path);
    strcpy (f_loc, otdb_names[n]);

    __g_db_data.db = open (full_path, O_RDONLY);
    free (full_path);
    free (dir_path);
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

#define order_type_size(n) (sizeof(order_type_t)+(n-1)*sizeof(ivec2))
order_type_t *order_type_new (int n, mem_pool_t *pool)
{
    order_type_t *ret = pom_push_size (pool, order_type_size(n));
    ret->n = n;
    ret->id = -1;
    return ret;
}

order_type_t *order_type_copy (mem_pool_t *pool, order_type_t *ot)
{
    uint64_t size = order_type_size(ot->n);
    order_type_t *ret = pom_push_size (pool, size);
    memcpy (ret, ot, size);
    return ret;
}

#define convex_ot(ot) convex_ot_scale (ot, 255/2)
void convex_ot_scale (order_type_t *ot, int radius)
{
    assert (ot->n >= 3);
    double theta = (2*M_PI)/ot->n;
    ot->id = 0;
    int i;
    for (i=1; i<=ot->n; i++) {
        ot->pts[i-1].x = (int)((double)radius*(1+cos(i*theta)));
        ot->pts[i-1].y = (int)((double)radius*(1+sin(i*theta)));
    }
}

void increase_duplicated_coords (order_type_t *ot, int_key_t *key_array, int coord)
{
    int i;
    for (i=0; i<ot->n; i++) {
        key_array[i].key = ot->pts[i].E[coord];
        key_array[i].origin = i;
    }
    sort_int_keys (key_array, ot->n);

    int last_val = key_array[0].key;
    for (i=1; i<ot->n; i++) {
        if (last_val == key_array[i].key) {
            ot->pts[key_array[i].origin].E[coord]++;
        }
        last_val = key_array[i].key;
    }
}

void convex_ot_searchable (order_type_t *ot)
{
    convex_ot_scale (ot, 65535/2);

    int_key_t key_array[ot->n];
    increase_duplicated_coords (ot, key_array, VECT_X);
    increase_duplicated_coords (ot, key_array, VECT_Y);
}

order_type_t* order_type_from_id (int n, uint64_t ot_id)
{
    order_type_t *res = order_type_new (n, NULL);
    if (ot_id == 0 && n>10) {
        convex_ot_searchable (res);
    } else {
        assert (n<=10);
        open_database (n);
        db_seek (res, ot_id);
    }
    return res;
}

void print_order_type (order_type_t *ot)
{
    int i = 0;
    if (ot->id != -1) {
        printf ("id: %"PRIu32"\n", ot->id);
    } else {
        printf ("id: unknown\n");
    }
    while (i < ot->n) {
        printf ("(%"PRIi64", %"PRIi64")\n", ot->pts[i].x, ot->pts[i].y);
        i++;
    }
    printf ("\n");
}

typedef struct {
    int n;
    int size;
    int8_t triples [1];
} ot_triples_t;

void ot_triples_print (ot_triples_t *tr) {
    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<tr->n; i++) {
        for (j=0; j<tr->n; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<tr->n; k++) {
                if (k==j || k==i) {
                    continue;
                }
                printf ("id:%d (%d, %d, %d) = %d\n", trip_id, i, j, k, tr->triples[trip_id]);
                trip_id ++;
            }
        }
    }
    printf ("\n");
}

ot_triples_t *ot_triples_new (order_type_t *ot, mem_pool_t *pool)
{
    ot_triples_t *ret;
    uint32_t size = ot->n*(ot->n-1)*(ot->n-2);
    if (pool) {
         ret = mem_pool_push_size (pool, sizeof(ot_triples_t)+(size-1)*sizeof(int8_t));
    } else {
        ret = malloc(sizeof(ot_triples_t)+(size-1)*sizeof(int8_t));
    }
    ret->n = ot->n;
    ret->size = size;

    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<ot->n; i++) {
        for (j=0; j<ot->n; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<ot->n; k++) {
                if (k==j || k==i) {
                    continue;
                }
                ret->triples[trip_id] = left_i(ot->pts[i], ot->pts[j], ot->pts[k]) ? 1 : -1;
                trip_id++;
            }
        }
    }
    return ret;
}

ot_triples_t *ps_triples_new (dvec2 *points, int len, mem_pool_t *pool)
{
    ot_triples_t *ret;
    uint32_t size = len*(len-1)*(len-2);
    if (pool) {
         ret = mem_pool_push_size (pool, sizeof(ot_triples_t)+(size-1)*sizeof(int8_t));
    } else {
        ret = malloc(sizeof(ot_triples_t)+(size-1)*sizeof(int8_t));
    }
    ret->n = len;
    ret->size = size;

    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<len; i++) {
        for (j=0; j<len; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<len; k++) {
                if (k==j || k==i) {
                    continue;
                }
                ret->triples[trip_id] = left(points[i], points[j], points[k]) ? 1 : -1;
                trip_id++;
            }
        }
    }
    return ret;
}

bool ot_triples_are_equal (ot_triples_t *tr_1, ot_triples_t *tr_2) {
    if (tr_1->size != tr_2->size) {
        return false;
    }

    int i;
    for (i=0; i<tr_1->size; i++) {
        if (tr_1->triples[i] != tr_2->triples[i]) {
            return false;
        }
    }
    return true;
}

// TODO: This is very slow.
void triple_from_id (int n, int id, int *a, int *b, int *c)
{
    uint32_t trip_id = 0;
    int i, j, k;
    for (i=0; i<n; i++) {
        for (j=0; j<n; j++) {
            if (j==i) {
                continue;
            }
            for (k=0; k<n; k++) {
                if (k==j || k==i) {
                    continue;
                }
                if (trip_id == id) {
                    *a=i;
                    *b=j;
                    *c=k;
                }
                trip_id ++;
            }
        }
    }
}

void print_triple (int n, int id) {
    int a=0, b=0, c=0;
    triple_from_id (n, id, &a, &b, &c);
    printf ("id:%d (%d, %d, %d)\n", id, a, b, c);
}

void print_differing_triples (int n, uint64_t ot_id_1, uint64_t ot_id_2)
{
    open_database (n);
    order_type_t *ot_1 = order_type_new (n, NULL);
    db_seek (ot_1, ot_id_1);
    ot_triples_t *ot_triples_1 = ot_triples_new (ot_1, NULL);

    order_type_t *ot_2 = order_type_new (n, NULL);
    db_seek (ot_2, ot_id_2);
    ot_triples_t *ot_triples_2 = ot_triples_new (ot_2, NULL);

    int32_t trip_id = 0;
    while (trip_id < ot_triples_1->size) {
        if (ot_triples_1->triples[trip_id] != ot_triples_2->triples[trip_id]) {
            print_triple (n, trip_id);
        }
        trip_id++;
    }
}

