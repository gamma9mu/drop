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
    int delete = 0;
    int list = 0;

    TCBDB * db;

    progname = argv[ 0 ];

    while (( c = (char)getopt( argc, argv, "hlLf:d:" ) ) > 0 )
    {
        switch ( c )
        {
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
            case 'L':
                list = 2;
                break;
            case 'l':
                list = 1;
                break;
            case '?':
            default:
                usage();
                break;
        }
    }

    if ( argv[ optind ] )
        key = argv[ optind ];

    if ( file == NULL ) file = get_db_location();
    db = tcbdbnew();
    if ( ! tcbdbopen( db, file, BDBOWRITER|BDBOCREAT|BDBOREADER ) )
    {
        int err = tcbdbecode( db );
        fprintf( stderr, "Could not open database: %s\n:%s\n", file, tcbdberrmsg( err ) );
        exit( -1 );
    }

    if ( list )
        list_keys( db, list - 1 );
    else if ( delete && key )
        delete_entry( db, key );
    else if ( key )
        print_entry( db, key );
    else
        interactive( db );

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
        exit( -1 );
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
        value = readline( ": " );

    if ( ! tcbdbputkeep2( db, key, value ) )
    {
        int err = tcbdbecode( db );
        char * resp = tcbdbget2( db, key );
        if ( ! resp )
        {
            fprintf( stderr, "Could not write: %s\n", tcbdberrmsg( err ) );
            exit ( -1 );
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
                exit ( -1 );
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
        exit( 0 );
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
        exit( 0 );
    }
    fprintf( stdout, "%s\n", value );
    free( value );
}

/* Print usage information. */
void usage( void )
{
    fprintf( stderr, "Usage: %s [-h][-l|L] [-f database_file] [-d] [key]\n\n", progname );
    fprintf( stderr, "If only 'key' is specified, the matching data is printed to stdout.\n\n" );
    fprintf( stderr, "\t-h       Print this message.\n" );
    fprintf( stderr, "\t-f file  Specifies an alternate database file.\n" );
    fprintf( stderr, "\t-l       List the available keys.\n" );
    fprintf( stderr, "\t-L       List the available keys and their values.\n" );
    fprintf( stderr, "\t-d key   Delete the entry specified by 'key'.\n" );
    fprintf( stderr, "\n\nThe latest '-l' or '-L' takes precedence.\n" );
    fprintf( stderr, "The key is one word only.  If multiple words are entered,"
        " only the first is used.\n" );
    exit( 0 );
}
