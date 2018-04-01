/*
 * Copiright (C) 2018 Santiago León O.
 */

struct config_t {
    int *n_values;
    uint32_t n_values_size;
    char *database_source;
    char *database_location;
};

char* get_next_key (char *c, char **key, uint32_t *key_size, char **val, uint32_t *val_size, bool *success)
{
    *success =  true;
    c = consume_spaces (c);
    while (*c == '#' || *c == '\n') {
        c = consume_line (c);
        c = consume_spaces (c);
    }

    *key = c;
    *key_size = 0;
    while (*c != ' ' && *c !='=' && *c != '\t') {
        c++;
        (*key_size)++;
    }

    c = consume_spaces (c);
    if (*c == '=') {
        c++; // Consume '='
        c = consume_spaces (c);

        *val = c;
        *val_size = 0;
        while (*c != '\n' && *c !=' ') {
            c++;
            (*val_size)++;
        }
    } else {
        *success = false;
    }

    return consume_line (c);
}

#define C_STR(str) str,((str)!=NULL?strlen(str):0)

static inline
bool strneq (const char *str1, uint32_t str1_size, const char* str2, uint32_t str2_size)
{
    if (str1_size != str2_size) {
        return false;
    } else {
        return (strncmp (str1, str2, str1_size) == 0);
    }
}

char* consume_character (char *str, char c, bool *failed)
{
    str = consume_spaces (str);
    if (*str != c) {
        *failed = true;
        return str;
    } else {
        str++; // consume c
        return str;
    }
}

void parse_int_list (mem_pool_t *pool, char *str, uint32_t str_size, int **res, uint32_t *res_size)
{
    int_dyn_arr_t found = {0};
    str = consume_spaces (str);
    bool is_list_end = false;
    *res_size = 0;
    while (*str != '\0' && *str != '\n' && !is_list_end) {
        char *int_end;
        int n = strtol (str, &int_end, 0);
        int_dyn_arr_append (&found, n);
        str = consume_character (int_end, ',', &is_list_end);
        (*res_size)++;
    }

    *res = (int*)pom_dup (pool, found.data, sizeof(int)*found.size);
    int_dyn_arr_destroy (&found);
}

struct config_t* read_config (mem_pool_t *pool)
{
    mem_pool_t local_pool = {0};
    struct config_t *res = pom_push_size (pool, sizeof (struct config_t));

    char *full_path = sh_expand ("~/.ps_viewer/viewer.conf", &local_pool);
    char *conf_file = full_file_read (&local_pool, full_path);

    bool success = true;
    if (conf_file) {
        char *c = conf_file;
        while (*c) {
            char *key, *val;
            uint32_t key_size, val_size;
            c = get_next_key (c, &key, &key_size, &val, &val_size, &success);
            if (!success) {
                break;
            }
            if (strneq (key, key_size, C_STR("N"))) {
                 parse_int_list (pool, val, val_size, &res->n_values, &res->n_values_size);

            } else if (strneq(key, key_size, C_STR("DatabaseSource"))) {
                res->database_source = (char*)pom_strndup (pool, val, val_size);

            } else if (strneq(key, key_size, C_STR("DatabaseLocation"))) {
                res->database_location = (char*)pom_strndup (pool, val, val_size);
            }
        }
    } else {
        success = false;
    }

    if (!success) {
        printf ("Error(s) reading configuration file.\n");
    }

    if (res->n_values == NULL) {
        printf ("Using default configuration value: N = 3,4,5,6,7,8,9,10\n");
        int n[] = {3,4,5,6,7,8,9,10};
        res->n_values = (int*)pom_dup (pool, n, ARRAY_SIZE(n));
    }

    if (res->database_source == NULL) {
        char *s = "http://www.ist.tugraz.at/aichholzer/research/rp/triangulations/ordertypes/data/";
        printf ("Using default configuration value: DatabaseSource = %s\n", s);
        res->database_source = (char*)pom_strndup (pool, C_STR(s));
    }

    if (res->database_location == NULL) {
        char *s = "~/.ps_viewer/";
        printf ("Using default configuration value: DatabaseLocation = %s\n", s);
        res->database_location = (char*)pom_strndup (pool, C_STR(s));
    }

    return res;
}
