/* drop.c
 * Written by Brian A. Guthrie
 * Distributed under 3-clause BSD license.  See LICENSE file for the details.
 */

#define _XOPEN_SOURCE 500

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <readline/readline.h>

#include "db.h"
#include "db_util.h"

#ifdef X11
#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

enum Operation { ADD, DELETE, LIST, FULL_LIST, PRINT };
enum TransferType { CONSOLE, READLINE,
#ifdef X11
XSELECTION_PRIMARY, XSELECTION_CLIPBOARD
#endif
};
enum ListingType { KEYS_ONLY = 0, KEYS_AND_ENTRIES = 1 };

typedef struct {
    enum Operation operation;
    enum TransferType transfer_type;
    char *key;
} options;

struct ExtensionMap {
    const char *ext;
    const char *type;
};


static void  parse_options(int ct, char **op, options *options);
static void  add(struct DbInterface*, void*, options*);
static void  delete(struct DbInterface*, void*, const char*);
static void  list(struct DbInterface*, void*, enum ListingType);
static void  print(struct DbInterface*, void*, options*);
static char *get_db_location(void);
static void  usage(void);

static get_interface_func load_support(char*);
static char *get_application_path(void);
static int is_link(const char*);

#ifdef X11
static void  init_x_win(enum TransferType destination);
static void  final_x_win(void);
static char *read_X_selection(options *opt);
static void  set_X_selection(options *opt, char *text);
static void  xdie(char *message);
static Time  get_X_timestamp(void);

static Display *d = NULL;
static Window w = 0;
static Atom selection_atom;
static Atom dest_atom;
static Atom XA_UTF8_STRING;
#endif

static struct ExtensionMap extension_map[] = {
    { "tcb", "tcbdb" },
    { "dbm", "gdbm" }
};

static char *progname;

int
main(int argc, char *argv[]) {
    char *file;
    database db;
    options opt;
    struct DbInterface *dbi;
    progname = argv[0];

    parse_options(argc, argv, &opt);

    file = get_db_location();
    dbi = load_support(file)();
    if ((db = dbi->open(file)) == NULL) {
        int err = dbi->get_errno(db);
        fprintf(stderr, "Could not open database: %s\n:%s\n", file,
            dbi->strerror(err));
        exit(EXIT_FAILURE);
    }
    free(file);

    switch (opt.operation) {
        case ADD:
            add(dbi, db, &opt);
            break;
        case DELETE:
            delete(dbi, db, opt.key);
            break;
        case PRINT:
            print(dbi, db, &opt);
            break;
        case LIST:
            list(dbi, db, KEYS_ONLY);
            break;
        case FULL_LIST:
            list(dbi, db, KEYS_AND_ENTRIES);
            break;
    }

    if (!dbi->close(db)) {
        fprintf(stderr, "Error closing database. Continuing, since I'm out of "
                "ideas...\n");
    }
    free(dbi);
    return EXIT_SUCCESS;
}

static void
parse_options(int ct, char **op, options *options_out)
{
    memset(options_out, 0, sizeof(options));

    options_out->operation = PRINT;
    options_out->transfer_type = READLINE;

    if (ct == 1 || strcmp("l", op[1]) == 0 || strcmp("list", op[1]) == 0)
    {
        options_out->operation = LIST;
    } else if (strcmp("h", op[1]) == 0 || strcmp("-h", op[1]) == 0
        || strcmp("help", op[1]) == 0 || strcmp("--help", op[1]) == 0) {
        usage();
    } else if (strcmp("f", op[1]) == 0 || strcmp("fulllist", op[1]) == 0) {
        options_out->operation = FULL_LIST;
    } else if (strcmp("a", op[1]) == 0 || strcmp("add", op[1]) == 0) {
        options_out->operation = ADD;
    } else if (strcmp("d", op[1]) == 0 || strcmp("delete", op[1]) == 0) {
        options_out->operation = DELETE;
#ifdef X11
    } else if (strncmp("xa", op[1], 2) == 0 || strncmp("xadd", op[1], 4) == 0) {
        options_out->operation = ADD;
        if (op[1][strlen(op[1])] == 'c')
            options_out->transfer_type = XSELECTION_CLIPBOARD;
        else
            options_out->transfer_type = XSELECTION_PRIMARY;
    } else if (strncmp("xp", op[1], 2) == 0 || strncmp("xprint", op[1], 6) == 0) {
        options_out->operation = PRINT;
        if (op[1][strlen(op[1])] == 'c')
            options_out->transfer_type = XSELECTION_CLIPBOARD;
        else
            options_out->transfer_type = XSELECTION_PRIMARY;
#endif
    } else if (ct == 2) {
        options_out->operation = PRINT;
        options_out->key = op[1];
    }

    if (options_out->operation != LIST && options_out->operation != FULL_LIST && options_out->operation != PRINT)
    {
        if (ct != 3) usage();
        options_out->key = op[2];
    }

}

#ifndef _POSIX_PATH_MAX
/* Default on my system...*/
#define _POSIX_PATH_MAX 256
#endif

/* Delete the entry specified by key. */
static void
delete(struct DbInterface *dbi, void *db, const char *key) {
    normalize_key(key);

    if (! dbi->delete(db, key)) {
        fprintf(stderr, "Could not delete '%s': %s\n", key, 
                dbi->strerror(dbi->get_errno(db)));
    }
}

/* Create a string for the DB location and fill it. The caller is responsible
 * for freeing the string.
 */
static char *
get_db_location() {
    DIR *dir;
    struct dirent *de;
    bool found = false;
    char *location;
    char *prefix = "drop.";
    char *dirpath = getenv("XDG_DATA_HOME");
    size_t len = 0;
    if (dirpath == NULL) {
        dirpath = getenv("HOME");
        prefix = ".drop.";
    }

    if ((dir = opendir(dirpath)) == NULL) {
        fprintf(stderr, "Could not open directory: \"%s\": %s\n", dirpath,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    while ((de = readdir(dir)) != NULL) {
        if (strstr(de->d_name, prefix) != NULL) {
            found = true;
            break;
        }
    }

    if (!found) {
        if (errno) {
            fprintf(stderr, "Error while scanning \"%s\": %s\n", dirpath,
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        prefix = "drop.dbm";
    } else {
        prefix = de->d_name;
    }

    len = strlen(prefix) + strlen(dirpath) + 1;
    if (len > _POSIX_PATH_MAX) {
        fprintf(stderr, "get_db_location: path name is impossible.\n");
        exit(EXIT_FAILURE);
    }

    ++len; /* pad for a null */
    if ((location = malloc(len)) == NULL) {
        fprintf(stderr, "get_db_location: malloc failed.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(location, len, "%s/%s", dirpath, prefix);

    if (closedir(dir) != 0) {
        fprintf(stderr, "Could not close directory: \"%s\": %s\n", dirpath,
                strerror(errno));
    }

    return location;
}

static void
add(struct DbInterface *dbi, void *db, options *opt) {
    char *key = opt->key;
    enum TransferType dest = opt->transfer_type;
    char *value = NULL;

    normalize_key(key);

#ifdef X11
    if (dest == XSELECTION_PRIMARY || dest == XSELECTION_CLIPBOARD) {
        value = read_X_selection(opt);
    } else {
#endif
        while (! value || ! *value) {
            value = readline("   : ");
        }
#ifdef X11
    }
#endif

    if (!dbi->try_store(db, key, value)) {
        int err = dbi->get_errno(db);
        char *resp = dbi->fetch(db, key);
        if (! resp) {
            fprintf(stderr, "Could not write: %s\n", dbi->strerror(err));
            return;
        }
        free(resp);
        resp = NULL;

        resp = readline("Overwrite? [y/N] ");
        if (resp && (resp[ 0 ] == 'y' || resp[ 0 ] == 'Y')) {
            if (!dbi->store(db, key, value)) {
                fprintf(stderr, "Could not write: %s\n",
                        dbi->strerror(dbi->get_errno(db)));
                return;
            }
        }
        free(resp);
    }

    free(value);
}

/* List the keys of the current entries. */
static void
list(struct DbInterface *dbi, void *db, enum ListingType full) {
    char *key = NULL;
    char *value = NULL;
    void *cur = dbi->create_cursor(db);
    if (!dbi->cursor_first(db, &cur)) {
        fprintf(stdout, "Database is empty.\n");
        return;
    }

    do {
        if ((key = dbi->cursor_key(db, &cur)) != NULL) {
            fputs(key, stdout);
            if (full == KEYS_AND_ENTRIES) {
                if ((value = dbi->cursor_value(db, &cur)) != NULL) {
                    fputs(": ", stdout);
                    size_t keylen = strlen(key);
                    if (keylen < 10) {
                        for (int i = 10 - keylen; i > 0; --i)
                            fputc(' ', stdout);
                    }
                    fputs(value, stdout);
                    free(value);
                }
            }
            fputc('\n', stdout);
            free(key);
        }
    } while (dbi->cursor_next(db, &cur));
    dbi->destroy_cursor(&cur);
}

/* Print the entry specified by key to stdout. */
static void
print(struct DbInterface *dbi, void *db, options *opt) {
    char *key = opt->key;
    enum TransferType dest = opt->transfer_type;
    char *value = NULL;

    if (key == NULL){
        return;
    }
    normalize_key(key);

    value = dbi->fetch(db, key);
    if (! value) {
        fprintf(stderr, "'%s' does not exist.\n", key);
        return;
    }
#ifdef X11
    if (dest == XSELECTION_PRIMARY || dest == XSELECTION_CLIPBOARD) {
        set_X_selection(opt, value);
    } else {
#endif
        fprintf(stdout, "%s\n", value);
#ifdef X11
    }
#endif
    free(value);
}

static get_interface_func
load_support(char *db_file) {
    const char *suffix, *type = NULL;
    char libpath[_POSIX_PATH_MAX];
    void *lib, *load;
    get_interface_func get_interface;

    if ((suffix = strrchr(db_file, '.')) == NULL) {
        type = "gdbm";
    } else {
        int items = (sizeof(extension_map) / sizeof(struct ExtensionMap));
        ++suffix;
        for (int i = 0; i < items; ++i) {
            if (strcmp(extension_map[i].ext, suffix) == 0) {
                type = extension_map[i].type;
            }
        }
        if (type == NULL) {
            type= "gdbm";
        }
    }

    char *basepath = get_application_path();
    snprintf(libpath, 64, "%s/db_%s.so", basepath, type);
    free(basepath);

    if ((lib = dlopen(libpath, RTLD_LAZY)) == NULL) {
        fprintf(stderr, "Could not load database support library: %s\n",
                dlerror());
        exit(EXIT_FAILURE);
    }
    if ((load = dlsym(lib, "get_interface")) == NULL) {
        fprintf(stderr, "Error loading database support: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    *(void**) (&get_interface) = load;
    return get_interface;
}

static char *
get_application_path() {
    char *apath;
    char *slash = strrchr(progname, '/');
    if (slash != NULL) {
        apath = strdup(progname);
        apath[slash - progname + 1] = '\0';
        return apath;
    }
    char *path = getenv("PATH");
    char *p = strtok(path, ":");
    size_t baselen = strlen(progname);
    while (p != NULL) {
        char *apath = malloc(baselen + strlen(p) + 2);
        if (apath == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        snprintf(apath, baselen + strlen(p) + 2, "%s/%s", p, progname);

        if (access(apath, F_OK) == 0) {
            if (is_link(apath)) {
                ssize_t len;
                char lnk[_POSIX_PATH_MAX];
                if ((len = readlink(apath, lnk, _POSIX_PATH_MAX)) == -1) {
                    perror("readlink");
                    exit(EXIT_FAILURE);
                }
                lnk[len - baselen] = 0;
                p = strdup(lnk);
                continue;
            }
            free(apath);
            break;
        }
        free(apath);
        apath = NULL;
        p = strtok(NULL, ":");
    }
    return strdup(p);
}

static int
is_link(const char *file) {
    struct stat s;
    if (lstat(file, &s) != 0) {
        perror("Error in stat");
        exit(EXIT_FAILURE);
    }
    return S_ISLNK(s.st_mode);
}

/* Print usage information. */
static void
usage(void)
{
    fprintf(stderr,
        "Usage: %s [command | key]\n"
        "\n"
        "If only 'key' is specified, the matching data is printed to stdout.  "
        "If no\n"
        "options are given, a list of keys is printed.\n"
        "\n"
        "\ta[dd]       <KEY> Add an item at KEY\n"
        "\td[elete]    <KEY> Delete item at KEY\n"
        "\tf[ulllist]        List all keys with their associated data.\n"
        "\th[elp]            Print this message.\n"
        "\tl[ist]            List all keys.\n"
        "\txa[dd][c]   <KEY> Add and item at KEY from an X selection buffers\n"
        "\txp[rint][c] <KEY> Insert the data at KEY an the X selection buffer.\n"
        "\n"
        "For xadd and xprint, the optional trailing 'c' specifies the CLIPBOARD"
        " selection\nbuffer should be used.  Otherwise, PRIMARY is used.\n"
        "\n",
        progname);
    exit(0);
}

#ifdef X11

/* Setup an X windows connection and the CLIPBOARD XAtom.
 */
static void
init_x_win(enum TransferType destination)
{
    setlocale(LC_CTYPE, "");
    d = XOpenDisplay(NULL);
    if (d == NULL)
        xdie("Could not open display\n");

    if (destination == XSELECTION_PRIMARY)
        selection_atom = XA_PRIMARY;
    else if (destination == XSELECTION_CLIPBOARD)
        selection_atom = XInternAtom(d, "CLIPBOARD", False);
    else
        xdie("Unknown selection atom.\n");

    dest_atom = XInternAtom(d, "DROP_CLIP", False);
    XA_UTF8_STRING = XInternAtom(d, "UTF8_STRING", False);

    w = XCreateSimpleWindow(d, RootWindow(d, DefaultScreen(d)), 0, 0, 1, 1, 0,
                            BlackPixel(d, DefaultScreen(d)),
                            WhitePixel(d, DefaultScreen(d)));
    if (w == BadAlloc || w == BadMatch || w == BadValue || w == BadWindow)
    {
        w = 0;
        xdie("XCreateSimpleWindow.\n");
    }
}

/* Cleanup the X windows connection.
 */
static void
final_x_win(void)
{
    if (w) XDestroyWindow(d, w);
    if (d != NULL) XCloseDisplay(d);
    exit(EXIT_SUCCESS);
}

/* Get the current X timestamp
 */
static Time
get_X_timestamp(void)
{
    int res;
    XEvent e;

    XSelectInput(d, w, PropertyChangeMask);
    res = XChangeProperty(d, w, selection_atom, XA_STRING, 8, PropModeAppend,
        0, 0);
    if (res == BadAlloc || res == BadAtom || res == BadMatch || res == BadValue
        || res == BadWindow) xdie("Local XChangeProperty.\n");

    do
    {
        XNextEvent(d, &e);
    } while (e.type != PropertyNotify);

    return e.xproperty.time;
}

/* Read the current X selection in and return it.
 * On an X error, a message will print and the program will exit. With other
 * problems, NULL is returned.  The string returned will be null-terminated.
 */
static char *
read_X_selection(options *opt)
{
    char *data = NULL;
    XEvent e;
    init_x_win(opt->transfer_type);
    Time t = get_X_timestamp();

    /* Request the data and wait for notification */
    XConvertSelection(d, selection_atom, XA_UTF8_STRING, dest_atom, w, t);
    for (;;)
    {
        XNextEvent(d, &e);
        if (e.type == SelectionNotify) break;
    }
    if (e.xselection.property == None)
    {
        final_x_win();
        return NULL;
    }

    /* Pull out the data */
    Atom type;
    int fmt;
    unsigned long num, after;
    int ret = XGetWindowProperty(d, w, dest_atom, 0L, 1024L, False,
                                 AnyPropertyType, &type, &fmt, &num, &after,
                                 (unsigned char **) &data);
    if (ret != Success) xdie("XGetWindowProperty failed.\n");
    if (type == None) xdie("Property not set after paste notification");
    if (fmt != 8)
    {
        XFree(data);
        xdie("Invalid format size received\n;");
    }

    /* Cleanup */
    XDeleteProperty(d, w, dest_atom);
    char *str = strdup(data);
    XFree(data);

    return str;
}

/* Offer up the contents of the current drop for an X selection
 */
static void
set_X_selection(options *opt, char *text)
{
    XEvent e;
    int res;
    init_x_win(opt->transfer_type);

    Time t = get_X_timestamp();

    /* Grab selection ownership */
    res = XSetSelectionOwner(d, selection_atom, w, t);
    if (res == BadAtom || res == BadWindow
        || w != XGetSelectionOwner(d, selection_atom))
        xdie("Could not control X selection.\n");

    /* Process events */
    for (;;)
    {
        XNextEvent(d, &e);
        if (e.type == SelectionClear)
        {
            if (e.xselectionclear.time > t) /* Lost selection ownership */
                final_x_win();
            else
                continue;
        }
        if (e.type != SelectionRequest) continue; /* Not a selection request */

        XSelectionRequestEvent *req =
            (XSelectionRequestEvent *) &e.xselectionrequest;

        /* Create respeonse event */
        XEvent resp;
        resp.xselection.type = SelectionNotify;
        resp.xselection.requestor = req->requestor;
        resp.xselection.selection = req->selection;
        resp.xselection.target = req->target;
        resp.xselection.time = req->time;
        if (req->property == None)
            resp.xselection.property = req->target;
        else
            resp.xselection.property = req->property;

        /* Send the selection buffer data */
        res = XChangeProperty(d, resp.xselection.requestor,
                              resp.xselection.property, resp.xselection.target,
                              8, PropModeReplace, (unsigned char *)text,
                              strlen(text) + 1);
        if (res == BadAlloc || res == BadAtom || res == BadMatch
            || res == BadValue || res == BadWindow)
        {
            resp.xselection.property = None;
            fprintf(stderr, "Foreign XChangeProperty.\n");
        }

        /* Send notice to the requesting application */
        res = XSendEvent(d, resp.xselection.requestor, True, NoEventMask,
            &resp);
        if (res == BadValue || res == BadWindow)
            xdie("XSendEvent failed.\n");

        final_x_win();
    }
}

/* Prints an optional error message, cleans up X connection and exits with
 * failure
 */
static void
xdie(char *message)
{
    if (message && *message) fputs(message, stderr);
    if (w) XDestroyWindow(d, w);
    if (d != NULL) XCloseDisplay(d);
    exit(EXIT_FAILURE);
}

#endif
