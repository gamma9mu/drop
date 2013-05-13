#ifndef DB_H__
#define DB_H__

typedef bool  (*close_func)(void*);
typedef void *(*create_cursor_func)(void*);
typedef bool  (*cursor_first_func)(void*, void*);
typedef bool  (*cursor_next_func)(void*, void*);
typedef char *(*cursor_key_func)(void*, void*);
typedef char *(*cursor_value_func)(void*, void*);
typedef bool  (*delete_func)(void*, const char*);
typedef void  (*destroy_cursor_func)(void*);
typedef int   (*errno_func)(void*);
typedef char *(*fetch_func)(void*, const char*);
typedef void *(*open_func)(const char*);
typedef bool  (*store_func)(void*, char*, char*);
typedef const char *(*strerror_func)(int);
typedef bool  (*try_store_func)(void*, char*, char*);

struct DbInterface {
    /* Basic */
    open_func open;
    close_func close;
    delete_func delete;
    fetch_func fetch;
    try_store_func try_store;
    store_func store;

    /* Cursors */
    create_cursor_func create_cursor;
    cursor_first_func cursor_first;
    cursor_next_func cursor_next;
    cursor_key_func cursor_key;
    cursor_value_func cursor_value;
    destroy_cursor_func destroy_cursor;

    /* Errors */
    errno_func get_errno;
    strerror_func strerror;
};

typedef void* database;
typedef void* cursor;
typedef struct DbInterface *(*get_interface_func)(void);

#endif /* DB_H__ */
