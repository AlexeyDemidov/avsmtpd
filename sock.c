
/*
 *   $Id: sock.c,v 1.1 2003-02-17 01:22:48 alexd Exp $
 *
 *   $Log: sock.c,v $
 *   Revision 1.1  2003-02-17 01:22:48  alexd
 *   moved some functions to smtp.c sock.c
 *
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "log.h"
#include "sock.h"


int sock_connect( struct sockaddr *addr) {
    /*
    flags = fcntl( s, F_GETFL, 0 );
    fcntl( s, F_SETFL, flags | O_NONBLOCK );

    FD_ZERO

    select( )
    */
    return -1;
}

int sock_listen( ) {
    return -1;
}

/*
 *   host:port
 *   /path/to/socket
 *
 */
struct sockaddr *parse_addr( const char *addr ) {
    static struct sockaddr saddr;

    struct sockaddr_in *saddr_in = (struct sockaddr_in *)&saddr;

    struct hostent *hp = NULL;

    char *ptr      = NULL;

    char *a        = NULL;
    char *port     = NULL;
    char *protocol = "tcp";

    if ( addr == NULL ) {
        error("parse_addr: invalid argument");
        return NULL;
    }
        
    a = strdup( addr );

    bzero( &saddr, sizeof(saddr) );

    if ( a[0] == '/') {
        error("parse_addr: unix sockets not supported now");
        return NULL;
    }
    
    saddr_in->sin_family = AF_INET;

    if ( (ptr = strchr( a, ':')) != NULL ) {
        *ptr = '\0';
        port = strdup( ptr + 1 ); 
    }

    if ( inet_aton( a, &saddr_in->sin_addr) == 0 ) {
        if ( (hp = gethostbyname(a)) == NULL ) {
            error("parse_addr: unknown host name %s", a );
            return NULL;
        }
        if ( hp->h_addrtype != AF_INET ) {
            error("parse_addr: unsuppored protocol for %s (%d)", addr, hp->h_addrtype );
            return NULL;
        }
        saddr_in->sin_addr = *(struct in_addr *)hp->h_addr;
    }

    if ( port == NULL ) {
        error("parse_addr: unknown port %s", addr );
        return NULL;
    }

    saddr_in->sin_port = htons( strtol( port, &ptr, 0 ) ); 

    if ( *ptr != '\0' ) { 
        struct servent *sp;

        sp = getservbyname( port, protocol );
        if ( sp == NULL ) {
            error("parse_addr: unknown service %s", port );
            return NULL;
        }
        saddr_in->sin_port = sp->s_port;
    }

    return &saddr;
}
