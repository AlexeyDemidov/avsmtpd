
/*
 *  DrWeb(R) daemon protocol support routines.
 *  Copyright (c) 2003 Alex L. Demidov
 */

/*
 *   $Id: drweb.c,v 1.6 2003-02-23 07:25:11 alexd Exp $
 *
 *   $Log: drweb.c,v $
 *   Revision 1.6  2003-02-23 07:25:11  alexd
 *   new sock_write syntax
 *
 *   Revision 1.5  2003/02/22 18:34:26  alexd
 *   replace send with sock_write
 *   rewrite dw_open with sock_connect
 *   free malloced strings
 *
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <config.h>

#include "drweb.h"
#include "log.h"
#include "sock.h"

#ifdef WITH_DMALLOC
#include "dmalloc.h"
#endif

char *drwebd_addr = "localhost:3000";

/*
 * "[inet:]hostname:port"
 * "[unix]:/path/to/socket"
 */

int dw_open( const char *addr ) {
#if 0
    struct sockaddr_in peer;
    struct sockaddr *daemon_addr;
    int s;

    bzero( &peer,  sizeof (peer));

    daemon_addr = parse_addr( addr );
    if ( daemon_addr == NULL ) {
        error( "dwopen::dw_open: bad drwebd socket address");
        return -1;
    }

    memcpy( &peer, daemon_addr, sizeof(peer) );

    s = socket( AF_INET, SOCK_STREAM, 0);

    if ( s < 0 ) { 
        Perror("dw_open::socket"); 
        return -1;
    } 

    
    if ( connect(s, (struct sockaddr *)&peer, sizeof (peer)) ) { 
        Perror("dw_open::connect"); 
        return -1;
    }
    return s;
#endif
    return sock_connect( addr );

}

int dw_close( int s) {
    return close(s);
}

int dw_write( int s, void *buf, size_t len ) {
    size_t rc;
    debug("dw_write: sending %d bytes to drwebd", len);

    rc = sock_write( s, buf, len, 120000 );
    debug("dw_write: sent %d bytes", rc);

    return rc != len;
}

int dw_writeint( int s, int value) {
    int v = htonl( value );
    return dw_write( s, &v, sizeof(int) );
}

int dw_readint( int s, int *value ) {
    int rc;

    rc = recv( s, value, sizeof( int ), 0);
    *value = ntohl(*value);

    return (rc != sizeof( int ));
}

char *dw_readline( int s) {
    char   *str;
    size_t  len;
    size_t  rc;

    rc = dw_readint( s, (int *)&len );
    if ( rc ) {
        error("dw_readline::dw_readint");
        return NULL;
    }

    debug( "dw_readline: str len = %d", len );

    str = malloc( len + 1);
    if ( str == NULL ) {
        error("dw_readline::malloc");
        return NULL;
    }
    
    rc = recv( s, str, len, 0 );
    if ( rc != len ) {
        error("dw_readline::recv");
        free( str );
        return NULL;
    }

    return str;
}

int dw_init( const char *addr ) {
    return 0;
}

int dw_shutdown() {
    return 0;
}

int  dw_getversion() {
    int s;
    int cmd_result;
    int rc;

    if ( (s = dw_open( drwebd_addr )) < 0) {
        error("can't open connection to drwebd at localhost:3000");
        return -1;
    }

    rc = dw_writeint(s, DRWEBD_VERSION_CMD); 
    if ( rc )
        goto error;

    debug("dw_getversion: write command complete");
    
    rc = dw_readint( s, &cmd_result );
    
    dw_close( s );

    return cmd_result;
error:
    debug("dw_getversion: can't write command to drwebd");

    dw_close( s );
    return -1;
}

char *dw_getid() {
    int s;
    int rc;

    char *id_str;

    if ( (s = dw_open( drwebd_addr )) < 0) {
        error("dw_getid: can't open connection to drwebd at localhost:3000");
        return NULL;
    }

    rc = dw_writeint(s, DRWEBD_IDSTRING_CMD); 
    if ( rc )
        goto error;
    
    debug("dw_getid: write command complete");
    
    id_str =  dw_readline( s );
    if ( id_str == NULL ) {
        error("dw_getid: failed reading response");
    }
    
    dw_close( s );
    return id_str;

error:
    debug("dw_getid: can't write command to drwebd");

    dw_close( s );
    return NULL;
}

void dw_getbaseinfo() {
    int s;
    int rc;
    int nbases;

    int i;

    char *id_str;

    if ( (s = dw_open( drwebd_addr )) < 0) {
        error("dw_getbaseinfo: can't open connection to drwebd at localhost:3000");
        return ;
    }

    rc = dw_writeint(s, DRWEBD_BASEINFO_CMD); 
    if ( rc )
        goto error;
    
    debug("dw_getbaseinfo: write command complete");

    rc = dw_readint( s, &nbases);

    notice("drwebd have %d bases loaded", nbases);

    for ( i = 0; i < nbases; i++) {
        int nviruses;

        id_str = dw_readline( s );
        dw_readint( s, &nviruses );
        notice( "base %s with %d viruses", id_str, nviruses );
        free( id_str );
    }

    return;

error:
    debug("dw_getbaseinfo: can't write command to drwebd");

    dw_close( s );
    return ;
}

int  dw_scan( void *data, size_t len ) {

    int rc;
    int s;

    int cmd_result;

    if ( (s = dw_open( drwebd_addr )) < 0) {
        error("can't open connection to drwebd at %s", drwebd_addr);
        return -1;
    }

    rc = dw_writeint( s, DRWEBD_SCAN_CMD );
    if ( rc ) 
        goto error;
    
    rc = dw_writeint( s, 0 ); /* flags */
    if ( rc ) 
        goto error;

    rc = dw_writeint( s, 0);
    if ( rc ) 
        goto error;
    
    rc = dw_writeint( s, len);
    if ( rc ) 
        goto error;

    rc = dw_write( s, data, len);
    if ( rc ) 
        goto error;

    debug("dw_scan: write command complete");
    
    rc = dw_readint( s, &cmd_result );

    dw_close( s );
    return (cmd_result & DERR_VIRUS) ;

error:
    debug("dw_scan: can't write command to drwebd");

    dw_close( s );
    return -1;

}
