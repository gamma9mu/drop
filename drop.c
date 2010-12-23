/* drop.c
 * Written by Brian A. Guthrie
 * Distributed under 3-clause BSD license.  See LICENSE file for the details.
 */

#define _XOPEN_SOURCE 500

#include <tcutil.h>
#include <tcbdb.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <limits.h>
#include <malloc.h>
#include <unistd.h>

#include <readline/readline.h>

char * progname;

void add_entry( TCBDB * db, const char * key );
void delete_entry( TCBDB * db, const char * key );
char * get_db_location( void );
void interactive( TCBDB * db );
void list_keys( TCBDB * db, int full );
void print_entry( TCBDB * db, const char * key );
void usage( void );

static void init_x_win(void);
static void final_x_win(void);
static char * read_X_selection( void );
static void set_X_selection( char * text );
static void xdie(char *message);

enum Operation { ADD, DELETE, LIST, FULL_LIST, INTERACTIVE, PRINT } op = PRINT;
enum TransferType { CONSOLE, READLINE, XSELECTION_PRIMARY,
    XSELECTION_CLIPBOARD } xfertype = READLINE;

int main( int argc, char* argv[] )
{
    char c;
    char * file = NULL;
    char * key = NULL;

    TCBDB * db;

    progname = argv[ 0 ];

    while (( c = (char)getopt( argc, argv, ":a:hilf:d:" ) ) > 0 )
    {
        switch ( c )
        {
            case 'a':
                op = ADD;
                key = optarg;
                break;
            case 'd':
                op = DELETE;
                key = optarg;
                break;
            case 'f':
                file = optarg;
                break;
            case 'h':
                usage();
                break;
            case 'i':
                xfertype = XSELECTION_PRIMARY;
                break;
            case 'I':
                xfertype = XSELECTION_CLIPBOARD;
                break;
            case 'l':
                op = ( op == LIST || op == FULL_LIST ) ? FULL_LIST : LIST;
                break;
            case ':':
                if ( optopt == 'a' )
                 {
                     op = ADD;
                     continue;
                 }
                 /* Fall through: -a allows for no arg */
            case '?':
            default:
                usage();
                break;
        }
    }

    if ( !key && argv[ optind ] )
        key = argv[ optind ];

    if ( op == ADD && ! ( key && *key ) )
        op = INTERACTIVE;

    if ( file == NULL ) file = get_db_location();
    db = tcbdbnew();
    if ( ! tcbdbopen( db, file, BDBOWRITER|BDBOCREAT|BDBOREADER ) )
    {
        int err = tcbdbecode( db );
        fprintf( stderr, "Could not open database: %s\n:%s\n", file, tcbdberrmsg( err ) );
        exit( -1 );
    }

    switch ( op )
    {
        case ADD:
            add_entry( db, key );
            break;
        case INTERACTIVE:
            interactive( db );
            break;
        case DELETE:
            delete_entry( db, key );
            break;
        case PRINT:
            print_entry( db, key );
            break;
        case LIST:
            list_keys( db, 0 );
            break;
        case FULL_LIST:
            list_keys( db, 1 );
            break;
    }

    if ( tcbdbclose( db ) == false )
    {
        fprintf( stderr, "Error closing database: %s\n", file );
        fprintf( stderr, "Continuing, since I'm out of ideas...\n" );
    }
    tcbdbdel( db );
    return 0;
}

#ifndef _POSIX_PATH_MAX
/* Default on my system...*/
#define _POSIX_PATH_MAX 256
#endif

/* Delete the entry specified by key. */
void delete_entry( TCBDB * db, const char * key )
{
    /* The key should only be one word... */
    char * space = strchr( key, ' ' );
    if ( space )
        *space = '\0';

    if ( !tcbdbout2( db, key ) )
    {
        int err = tcbdbecode( db );
        fprintf( stderr, "Could not delete '%s': %s\n", key, tcbdberrmsg( err) );
        return;
    }
}

/* Create a string for the DB location and fill it. The caller is responsible
 * for freeing the string.
 */
char * get_db_location()
{
    char * location;
    char * suffix = "/drop.tcb";
    char * dir = getenv( "XDG_DATA_HOME" );
    size_t len = 0;
    if ( dir == NULL )
    {
        dir = getenv( "HOME" );
        suffix = "/.drop.tcb";
    }

    len = strlen( suffix ) + strlen( dir );
    if ( len > _POSIX_PATH_MAX )
    {
        fprintf( stderr, "get_db_location: path name is impossible.\n" );
        exit( -1 );
    }

    ++len; /* pad for a null */
    location = (char *) malloc( len );
    if ( location == NULL )
    {
        fprintf( stderr, "get_db_location: malloc failed.\n" );
        exit( -1 );
    }

    snprintf( location, len, "%s%s", dir, suffix );
    return location;
}

void add_entry( TCBDB * db, const char * key )
{
    char * value = NULL;
    char * space = NULL;

    /* The key should only be one word... */
    space = strchr( key, ' ' );
    if ( space )
        *space = '\0';

    while ( ! value || ! *value )
        value = readline( "   : " );

    if ( ! tcbdbputkeep2( db, key, value ) )
    {
        int err = tcbdbecode( db );
        char * resp = tcbdbget2( db, key );
        if ( ! resp )
        {
            fprintf( stderr, "Could not write: %s\n", tcbdberrmsg( err ) );
            return;
        }
        free( resp );
        resp = NULL;

        resp = readline( "Overwrite? [y/N] " );
        if ( resp && ( resp[ 0 ] == 'y' || resp[ 0 ] == 'Y' ))
        {
            if ( !tcbdbput2( db, key, value ) )
            {
                err = tcbdbecode( db );
                fprintf( stderr, "Could not write: %s\n", tcbdberrmsg( err ) );
                return;
            }
        }
        free( resp );
    }

    free( value );
}

/* Interactively enter a key into the DB. */
void interactive( TCBDB * db )
{
    char * key = NULL;
    char * value = NULL;
    char * space = NULL;
    key = readline( "Key: " );

    /* Exit on an empty key. */
    if ( !key || !*key )
        return;

    /* The key should only be one word... */
    space = strchr( key, ' ' );
    if ( space )
        *space = '\0';

    while ( ! value || ! *value )
        value = readline( "   : " );

    if ( ! tcbdbputkeep2( db, key, value ) )
    {
        int err = tcbdbecode( db );
        char * resp = tcbdbget2( db, key );
        if ( ! resp )
        {
            fprintf( stderr, "Could not write: %s\n", tcbdberrmsg( err ) );
            return;
        }
        free( resp );
        resp = NULL;

        resp = readline( "Overwrite? [y/N] " );
        if ( resp && ( resp[ 0 ] == 'y' || resp[ 0 ] == 'Y' ))
        {
            if ( !tcbdbput2( db, key, value ) )
            {
                err = tcbdbecode( db );
                fprintf( stderr, "Could not write: %s\n", tcbdberrmsg( err ) );
                return;
            }
        }
        free( resp );
    }

    free( key );
    free( value );
}

/* List the keys of the current entries. */
void list_keys( TCBDB * db, int full )
{
    char * key = NULL;
    char * value = NULL;
    BDBCUR * cur = tcbdbcurnew( db );
    if ( ! tcbdbcurfirst( cur ) )
    {
        fprintf( stdout, "Database is empty.\n" );
        return;
    }

    do
    {
        key = tcbdbcurkey2( cur );
        if ( key )
        {
            fputs( key, stdout );
            if ( full )
            {
                value = tcbdbcurval2( cur );
                if ( value )
                {
                    fprintf( stdout, ":\t%s\n", value );
                    free( value );
                } else {
                    fputc( '\n', stdout );
                }
            } else {
                fputc( '\n', stdout );
            }
            free( key );
        }
    } while ( tcbdbcurnext( cur ) );
    tcbdbcurdel( cur );
}
/* Print the entry specified by key to stdout. */
void print_entry( TCBDB * db, const char * key )
{
    /* The key should only be one word... */
    char * space = strchr( key, ' ' );
    char * value = NULL;
    if ( space )
        *space = '\0';

    value = tcbdbget2( db, key );
    if ( ! value )
    {
        fprintf( stderr, "'%s' does not exist.\n", key );
        return;
    }
    fprintf( stdout, "%s\n", value );
    free( value );
}

/* Print usage information. */
void usage( void )
{
    fprintf( stderr,
"Usage: %s [-h] [-l] [-f database_file] [-d] [key]\n\n"
"If only 'key' is specified, the matching data is printed to stdout.  If no\n"
"options are given, a list of keys is printed.\n\n"
"\t-h        Print this message.\n"
"\t-a [key]  Add a value with 'key' as the key.\n"
"\t-f file   Specifies an alternate database file.\n"
"\t-l        List the available keys and their values.\n"
"\t-d key    Delete the entry specified by 'key'.\n\n",
        progname);

    fprintf(stderr,
"Transfer types:\n"
"\t-i        Transfer the entry using X PRIMARY selection.\n"
"\t-I        Transfer the entry using X CLIPBOARD selection.\n"
"\n\nThe key is one word only.  If multiple words are entered,"
" only the first is used as the key.\n");

    exit( 0 );
}

#include <locale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Time get_X_timestamp(void);

static Display *d = NULL;
static Window w = 0;
static Atom selection_atom;
static Atom dest_atom;
static Atom XA_UTF8_STRING;

/* Setup an X windows connection and the CLIPBOARD XAtom.
 */
static void
init_x_win(void)
{
    setlocale(LC_CTYPE, "");
    d = XOpenDisplay(NULL);
    if (d == NULL)
        xdie("Could not open display\n");

    if (xfertype == XSELECTION_PRIMARY)
        selection_atom = XA_PRIMARY;
    else if (xfertype == XSELECTION_CLIPBOARD)
        selection_atom = XInternAtom(d, "CLIPBOARD", False);
    else
        xdie("Unknown selection atom.\n");

    dest_atom = XInternAtom(d, "DROP_CLIP", False);
    XA_UTF8_STRING = XInternAtom(d, "UTF8_STRING", False);
 
    w = XCreateSimpleWindow(d, RootWindow(d, DefaultScreen(d)), 0, 0, 1, 1, 0,
        BlackPixel(d, DefaultScreen(d)), WhitePixel(d, DefaultScreen(d)));
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
    res = XChangeProperty(d, w, selection_atom, XA_STRING, 8, PropModeAppend, 0, 0);
    if (res == BadAlloc || res == BadAtom || res == BadMatch || res == BadValue
        || res == BadWindow) xdie("Local XChangeProperty.\n");

    do { XNextEvent(d, &e); } while (e.type != PropertyNotify);

    return e.xproperty.time;
}

/* Read the current X selection in and return it.
 * On an X error, a message will print and the program will exit. With other
 * problems, NULL is returned.  The string returned will be null-terminated.
 */
static char *
read_X_selection( void )
{
    char *data = NULL;
    XEvent e;
    init_x_win();
    Time t = get_X_timestamp();

    // Request the data and wait for notification
    XConvertSelection(d, selection_atom, XA_UTF8_STRING, dest_atom, w, t);
    for(;;) {
        XNextEvent(d, &e);
        if (e.type == SelectionNotify) break;
    }
    if (e.xselection.property == None)
    {
        final_x_win();
        return NULL;
    }

    // Pull out the data
    Atom type;
    int fmt;
    unsigned long num, after;
    int ret = XGetWindowProperty(d, w, dest_atom, 0L, 1024L, False,
        AnyPropertyType, &type, &fmt, &num, &after, 
        (unsigned char**) &data);
    if (ret != Success) xdie("XGetWindowProperty failed.\n");
    if (type == None) xdie("Property not set after paste notification");
    if (fmt != 8)
    {
        XFree(data);
        xdie("Invalid format size received\n;");
    }

    // Cleanup
    XDeleteProperty(d, w, dest_atom);
    char *str = strdup(data);
    XFree(data);

    return str;
}

/* Offer up the contents of the current drop for an X selection
 */
static void
set_X_selection( char * text )
{
    XEvent e;
    int res;
    init_x_win();

    Time t = get_X_timestamp();

    // Grab selection ownership
    res = XSetSelectionOwner(d, selection_atom, w, t);
    if (res == BadAtom || res == BadWindow || w != XGetSelectionOwner(d, selection_atom))
        xdie("Could not control X selection.\n");

    // Process events
    for(;;) {
        XNextEvent(d, &e);
        if (e.type == SelectionClear) {
            if (e.xselectionclear.time > t) // Lost selection ownership
                final_x_win();
            else
                continue;
        }
        if (e.type != SelectionRequest) continue; // Not a selection request

        XSelectionRequestEvent *req =
            (XSelectionRequestEvent*) &e.xselectionrequest;

        // Create respeonse event
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

        // Send the selection buffer data
        res = XChangeProperty(d, resp.xselection.requestor,
            resp.xselection.property, resp.xselection.target, 8,
            PropModeReplace, (unsigned char*)text, strlen(text) + 1);
        if (res == BadAlloc || res == BadAtom || res == BadMatch || res == BadValue
            || res == BadWindow)
        {
            resp.xselection.property = None;
            fprintf(stderr, "Foreign XChangeProperty.\n");
        }

        // Send notice to the requesting application
        res = XSendEvent(d, resp.xselection.requestor, True, 0L, &resp);
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
