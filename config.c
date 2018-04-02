/*
 * Copiright (C) 2018 Santiago León O.
 */

struct config_t {
    int *n_values;
    uint32_t num_n_values;
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

void parse_int_list_range (mem_pool_t *pool, char *str, uint32_t str_size, int **res, uint32_t *res_size,
                           int min_val, int max_val)
{
    int_dyn_arr_t found = {0};
    str = consume_spaces (str);
    bool is_list_end = false;
    *res_size = 0;
    while (*str != '\0' && *str != '\n' && !is_list_end) {
        char *int_end;
        int n = strtol (str, &int_end, 0);
        if (n < min_val || max_val < n) {
            printf ("Integer outside of valid range [%i - %i]\n", min_val, max_val);
        }
        int_dyn_arr_append (&found, n);
        str = consume_character (int_end, ',', &is_list_end);
        (*res_size)++;
    }

    *res = (int*)pom_dup (pool, found.data, sizeof(int)*found.size);
    int_dyn_arr_destroy (&found);
}

#define CONFIG_DIR "~/.ps_viewer/"
#define CONFIG_FILE CONFIG_DIR"viewer.conf"
struct config_t* read_config (mem_pool_t *pool)
{
    mem_pool_t local_pool = {0};
    struct config_t *res = pom_push_size (pool, sizeof (struct config_t));

    ensure_dir_exists (CONFIG_DIR);
    if (!path_exists (CONFIG_FILE)) {
        char *default_config = 
        "# Number of points. Can contain values between [3-10]\n"
        "N = 3,4,5,6,7,8,9,10\n"
        "\n"
        "# Where the database will be downloaded from\n"
        "DatabaseSource = http://www.ist.tugraz.at/aichholzer/research/rp/triangulations/ordertypes/data/\n"
        "\n"
        "# Where the database will be downloaded into. If files are already there then\n"
        "# we don't download anything.\n"
        "DatabaseLocation = "CONFIG_DIR"\n";
        full_file_write (default_config, strlen(default_config), CONFIG_FILE);
    }
    char *conf_file = full_file_read (&local_pool, CONFIG_FILE);

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
                 parse_int_list_range (pool, val, val_size, &res->n_values, &res->num_n_values, 3, 10);

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
        printf ("Using default configuration value: DatabaseSource = %s\n", DEFAULT_DB_SOURCE);
        res->database_source = (char*)pom_strndup (pool, C_STR(DEFAULT_DB_SOURCE));
    }

    if (res->database_location == NULL) {
        printf ("Using default configuration value: DatabaseLocation = %s\n", DEFAULT_DB_LOCATION);
        res->database_location = (char*)pom_strndup (pool, C_STR(DEFAULT_DB_LOCATION));
    }

    return res;
}
