/* drop.c
 * Written by Brian A. Guthrie
 * Distributed under 3-clause BSD license.  See LICENSE file for the details.
 */

#include <tcutil.h>
#include <tcbdb.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

int main( int argc, char* argv[] )
{
    char c;
    char * file = NULL;
    char * key = NULL;
    int add = 0;
    int gointeractive = 0;
    int delete = 0;
    int list = 0;

    TCBDB * db;

    progname = argv[ 0 ];

    while (( c = (char)getopt( argc, argv, ":a:ihilf:d:" ) ) > 0 )
    {
        switch ( c )
        {
            case 'a':
                add = 1;
                key = optarg;
                break;
            case 'd':
                key = optarg;
                delete = 1;
                break;
            case 'f':
                file = optarg;
                break;
            case 'h':
                usage();
                break;
            case 'l':
                list = 1;
                break;
            case 'i':
                gointeractive = 1;
                break;
            case ':':
                if ( optopt == 'a' )
                 {
                     add = 1;
                     continue;
                 }
                 /* Fall through: -a allows for no arg */
            case '?':
            default:
                usage();
                break;
        }
    }

    if ( argv[ optind ] )
        key = argv[ optind ];

    if ( add && ! ( key && *key ) )
    {
        add = 0;
        gointeractive = 1;
    }

    if ( file == NULL ) file = get_db_location();
    db = tcbdbnew();
    if ( ! tcbdbopen( db, file, BDBOWRITER|BDBOCREAT|BDBOREADER ) )
    {
        int err = tcbdbecode( db );
        fprintf( stderr, "Could not open database: %s\n:%s\n", file, tcbdberrmsg( err ) );
        exit( -1 );
    }

    if ( add )
        add_entry( db, key );
    else if ( gointeractive )
        interactive( db );
    else if ( delete && key )
        delete_entry( db, key );
    else if ( key )
        print_entry( db, key );
    else
        list_keys( db, list );

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

"Usage: %s [-h] [-i] [-l] [-f database_file] [-d] [key]\n\n"
"If only 'key' is specified, the matching data is printed to stdout.  If no\n"
"options are given, a list of keys is printed.\n\n"
"\t-h        Print this message.\n"
"\t-a [key]  Add a value with 'key' as the key, otherwise same as -i.\n"
"\t-f file   Specifies an alternate database file.\n"
"\t-i        Interactive entry.\n"
"\t-l        List the available keys and their values.\n"
"\t-d key    Delete the entry specified by 'key'.\n"
"\n\nThe key is one word only.  If multiple words are entered,"
" only the first is used.\n",

        progname );
    exit( 0 );
}
