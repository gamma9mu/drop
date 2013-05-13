#include <tcutil.h>
#include <tcbdb.h>

#include "db.h"

static bool  tcdb_close(void*);
static void *tcdb_create_cursor(void*);
static bool  tcdb_cursor_first(void*, void*);
static char *tcdb_cursor_key(void*, void*);
static bool  tcdb_cursor_next(void*, void*);
static char *tcdb_cursor_value(void*, void*);
static void  tcdb_destroy_cursor(void*);
static void *tcdb_open(const char*);

struct DbInterface *get_interface(void);

static bool
tcdb_close(void *db) {
    bool ret = tcbdbclose(db);
    tcbdbdel(db);
    return ret;
}

static void *
tcdb_create_cursor(void *db) {
    return tcbdbcurnew(db);
}

static bool
tcdb_cursor_first(void *db, void *cursor) {
    (void) db;
    return tcbdbcurfirst(cursor);
}

static char *
tcdb_cursor_key(void *db, void *cursor) {
    (void) db;
    return tcbdbcurkey2(cursor);
}

static bool
tcdb_cursor_next(void *db, void *cursor) {
    (void) db;
    return tcbdbcurnext(cursor);
}

static char *
tcdb_cursor_value(void *db, void *cursor) {
    (void) db;
    return tcbdbcurval2(cursor);
}

static void
tcdb_destroy_cursor(void *cursor) {
    tcbdbcurdel(cursor);
}

static void *
tcdb_open(const char *file) {
    TCBDB *db = tcbdbnew();
    if (!tcbdbopen(db, file, BDBOWRITER | BDBOCREAT | BDBOREADER)) {
        return NULL;
    }
    return db;
}

struct DbInterface *
get_interface() {
    struct DbInterface *dbint = malloc(sizeof(struct DbInterface));
    if (dbint == NULL) {
        return dbint;
    }
    dbint->open = (open_func) tcdb_open;
    dbint->close = (close_func) tcdb_close;
    dbint->get_errno = (errno_func) &tcbdbecode;
    dbint->strerror = (strerror_func) &tcbdberrmsg;
    dbint->delete = (delete_func) &tcbdbout2;
    dbint->fetch = (fetch_func) &tcbdbget2;
    dbint->try_store = (try_store_func) &tcbdbputkeep2;
    dbint->store = (store_func) &tcbdbput2;
    dbint->create_cursor = (create_cursor_func) &tcdb_create_cursor;
    dbint->destroy_cursor = (destroy_cursor_func) &tcdb_destroy_cursor;
    dbint->cursor_first = (cursor_first_func) &tcdb_cursor_first;
    dbint->cursor_next = (cursor_next_func) &tcdb_cursor_next;
    dbint->cursor_key = (cursor_key_func) &tcdb_cursor_key;
    dbint->cursor_value = (cursor_value_func) &tcdb_cursor_value;
    return dbint;
}
