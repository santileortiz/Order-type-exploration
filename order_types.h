/*
 * Copiright (C) 2017 Santiago León O.
 */
#if !defined(ORDER_TYPES_H)

#define DEFAULT_DB_LOCATION "http://www.ist.tugraz.at/aichholzer/research/rp/triangulations/ordertypes/data/"
#define DEFAULT_DB_SOURCE "~/.ps_viewer/"

typedef struct {
    int n;
    uint32_t id;
    ivec2 pts [1];
} order_type_t;

struct {
    int db;
    int n;
    int coord_size; //bits per coordinate in database
    int ot_size; //size of an order type in bytes (in the database)
    int eof_reached;
    uint64_t num_order_types;
    uint64_t indx;
    uint32_t buff[40];
    char *source;
    char *location;
} __g_db_data ;

int open_database (int n);
void db_next (order_type_t *ot);
void db_seek (order_type_t *ot, uint64_t id);
void db_prev (order_type_t *ot);
int db_is_eof ();

#define ORDER_TYPES_H
#endif
