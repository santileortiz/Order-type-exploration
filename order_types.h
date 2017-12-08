/*
 * Copiright (C) 2017 Santiago León O.
 */
#if !defined(ORDER_TYPES_H)

typedef struct {
    int n;
    uint32_t id;
    vect2i_t pts [1];
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
} __g_db_data ;

int open_database (int n);
void db_next (order_type_t *ot);
void db_seek (order_type_t *ot, uint64_t id);
void db_prev (order_type_t *ot);
int db_is_eof ();

#define ORDER_TYPES_H
#endif
