/*
 *  readcfg.c - config reading routines.
 *  Copyright (c) 1998 Alex L. Demidov
 */

/*
 *  $Id: readcfg.c,v 1.1 2003-02-17 01:22:48 alexd Exp $
 *
 *  $Log: readcfg.c,v $
 *  Revision 1.1  2003-02-17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *  Revision 1.3  2001/03/24 17:51:05  alexd
 *  Added RCS strings to .c files
 *
 *  Revision 1.2  1999/03/12 22:46:55  alexd
 *  some headers
 *
 *  Revision 1.1.1.1  1999/03/12 22:41:10  alexd
 *  imported fidod
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "readcfg.h"
#include "log.h"

const static char *rcsid = "$Id: readcfg.c,v 1.1 2003-02-17 01:22:48 alexd Exp $";
const static char *revision = "$Revision: 1.1 $";


int getvarstr( var_t *key , const char *str) {
    const char *p = str;
#if 0
    char **ptr = (char **)(key->var);
    char *name;
    char *value;
#endif

    if( strncasecmp(key->name, str, strlen(key->name) ) == 0 ) {
        while( p && *p && !isspace(*p))
            p++;
        while( p && *p && isspace(*p))
            p++;
        debug("found key (%s: %s)", key->name, p );

        *((char **)(key->var)) = strdup( p );
        return 1;
    }

#if 0
    /* get name */
    while( p && *p && !isspace(*p))
        p++;
    if( p )
        *p = 0;
    name =  strdup( p );
    /* get value */
    p++;
    while ( *p && isspace(*p) )
        p++;
    value = strdup( p );
#endif
    
    return 0;
}

int getvarbool( var_t *key , const char *str) {
    const char *p = str;

    if( strncasecmp(key->name, str, strlen(key->name) ) == 0 ) {
        while( p && *p && !isspace(*p))
            p++;
        while( p && *p && isspace(*p))
            p++;
        debug("found key (%s: %s)", key->name, p );
	
        *((int *)(key->var)) = (strcmp( p, "yes") == 0);
        return 1;
    }
    
    return 0;
}


void parse_config_line( const char *buf, size_t bufsize ) {

    static char *line = 0;
    char *p = 0;

    int next_line = 0;
    

    /* fgets DOES store \0 after last char in *buf */

    /* check if complete line read */
    if( (p = strrchr( buf, '\n' )) != NULL ) {
        *p = 0; /* trash newline char */
        
        /* check line continuation by \ */
        if( *(p - 1) == '\\' ) {
            *(p - 1) = 0;
            next_line = 1;
        }
        else
            next_line = 0;
    }
    else
        next_line = 1;

#if 0
    /* note! if \\ goes in comment it is ignored */
    if ( ( p = strrchr( buf, '\\') ) != NULL ) {
        *p = 0;
        next_line = 1;
    }
#endif
    
    if ( line ) { /* already have chunk of line */
        line = realloc( line, strlen(buf) + strlen(line) + 1 );
        if ( line == NULL ) {
            error("error allocating memory");
            return;
        }
        strcat( line, buf ); /* we have allocated enought space for  */
                             /* holding both chunks of line,         */
                             /* so, strcat should not be dangerous   */
                             /* (if strlen not lied us)              */

    }
    else  /* new line */
        line = strdup( buf );
    
    if ( next_line ) /* need next chunk of line       */
        return;      /* if we are at end of file      */
                     /* last line simply gets ignored */
    
    /* strip comments */
    if ( ( p = strchr( line, '#') ) != NULL )
        *p = 0;
    
    if( line ) { /* ok, have a complete line */
        int key = 0;
        char *p = line;
        debug("have complete config line: %s", line );
        /* strip leading spaces */
        while( *p && isspace(*p))
            p++;
        if ( *p ) {
            char *tmp = strdup(p);
            free( line );
            line = tmp;
        }

        /* strip trailing space */
        p = line + strlen(line) - 1;
        while( (p >= line) && isspace(*p)) {
            *p = 0;
            p--;
        }
        line = realloc(line, strlen(line) + 1 );
        
        /* get variable */
        for( key = 0; vars[key].name; key++ ) {
            if( vars[key].getvar( &(vars[key]), line ) )
                break;
        }
    }
    else
        error("error allocating memory");
    
    free( line );
    line = NULL;
}

int read_config() {
    
#define BUFSIZE 128
    
    char *buf;
    FILE *f;

    if( (buf = malloc(BUFSIZE)) == NULL) {
        error( "error allocating memory" );
        return 1;
    }

    debug("trying to open config file %s", config_file);
    f = fopen(config_file, "r");
    if ( !f ) {
        error( "error opening config file" );
        return 1;
    }
    while( fgets( buf, BUFSIZE, f) != NULL) {
        debug( "read from config: %s", buf );
        parse_config_line( buf, BUFSIZE );
    }
    fclose(f);
    return 0;
}
