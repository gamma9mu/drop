#define _XOPEN_SOURCE 500

#include <gdbm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "db.h"

static bool  gdbm_close_func(void*);
static void *gdbm_create_cursor(void*);
static bool  gdbm_cursor_first(void*, datum*);
static char *gdbm_cursor_key(void*, datum*);
static bool  gdbm_cursor_next(void*, datum*);
static char *gdbm_cursor_value(void*, datum*);
static void  gdbm_destroy_cursor(void*);
static int   gdbm_get_errno(void);
static void *gdbm_open_func(const char*);
static bool  gdbm_store_force(void*, char*, char*);
static bool  gdbm_store_try(void*, char*, char*);

struct DbInterface *get_interface(void);

static bool
gdbm_close_func(void *db) {
    gdbm_close(db);
    return true;
}

static void *
gdbm_create_cursor(void *db) {
    (void) db;
    return NULL;
}

static bool
gdbm_cursor_first(void *db, datum *gdbm_cursor) {
    *gdbm_cursor = gdbm_firstkey(db);
    return gdbm_cursor->dptr != NULL;
}

static char *
gdbm_cursor_key(void *db, datum *gdbm_cursor) {
    (void) db;
    return strdup(gdbm_cursor->dptr);
}

static bool
gdbm_cursor_next(void *db, datum *gdbm_cursor) {
    *gdbm_cursor = gdbm_nextkey(db, *gdbm_cursor);
    return gdbm_cursor->dptr != NULL;
}

static char *
gdbm_cursor_value(void *db, datum *gdbm_cursor) {
    datum ret = gdbm_fetch(db, *gdbm_cursor);
    return strdup(ret.dptr);
}

static void
gdbm_destroy_cursor(void *db) {
    (void) db;
}

static int
gdbm_get_errno() {
    return (int) gdbm_errno;
}

static void *
gdbm_open_func(const char *file) {
    return gdbm_open(file, 0, GDBM_WRCREAT,
            S_IRUSR | S_IWUSR, NULL);
}

static bool
gdbm_store_force(void *db, char *key, char *value) {
    datum k, v;
    int ret;
    k.dptr = key;
    k.dsize = strlen(key) + 1;
    v.dptr = value;
    v.dsize = strlen(value) + 1;
    ret = gdbm_store(db, k, v, GDBM_REPLACE);
    return ret == 0;
}

static bool
gdbm_store_try(void *db, char *key, char *value) {
    datum k, v;
    int ret;
    k.dptr = key;
    k.dsize = strlen(key) + 1;
    v.dptr = value;
    v.dsize = strlen(value) + 1;
    ret = gdbm_store(db, k, v, GDBM_INSERT);
    return ret == 0;
}

static struct DbInterface gdbm = {
    .open = (open_func) gdbm_open_func,
    .close = (close_func) gdbm_close_func,
    .get_errno = (errno_func) gdbm_get_errno,
    .strerror = (strerror_func) gdbm_strerror,
    .delete = (delete_func) gdbm_delete,
    .fetch = (fetch_func) gdbm_fetch,
    .try_store = gdbm_store_try,
    .store = gdbm_store_force,
    .create_cursor = (create_cursor_func) gdbm_create_cursor,
    .destroy_cursor = (destroy_cursor_func) gdbm_destroy_cursor,
    .cursor_first = (cursor_first_func) gdbm_cursor_first,
    .cursor_next = (cursor_next_func) gdbm_cursor_next,
    .cursor_key = (cursor_key_func) gdbm_cursor_key,
    .cursor_value = (cursor_value_func) gdbm_cursor_value
};

struct DbInterface *
get_interface() {
    struct DbInterface *dbint = malloc(sizeof(struct DbInterface));
    if (dbint == NULL) {
        return dbint;
    }
    memcpy(dbint, &gdbm, sizeof(struct DbInterface));
    return dbint;
}
