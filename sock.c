/*
 * $Id: sock.c,v 1.3 2003-02-22 18:38:42 alexd Exp $
 * 
 * $Log: sock.c,v $
 * Revision 1.3  2003-02-22 18:38:42  alexd
 * add sock_listen, sock_connect, sock_write, sock_read functions
 *
 * Revision 1.2  2003/02/17 20:42:53  alexd
 * some more clean up
 * Revision 1.1  2003/02/17 01:22:48  alexd 
 * moved some functions to smtp.c sock.c
 * 
 * 
 * 
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>

#include <config.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef WITH_DMALLOC
#include "dmalloc.h"
#endif

#include <fcntl.h>

#include "log.h"
#include "sock.h"

int sock_write( int s, void *buf, size_t len ) { 
    int rc;

#ifdef HAVE_POLL    
    struct pollfd pfd;

    int timeout = 5000; /* milliseconds */

    pfd.fd = s;
    pfd.events  = POLLOUT ;
    pfd.revents = 0;

    rc = poll( &pfd, 1, timeout );

    if ( rc == -1 ) {
        Perror("sock_write: poll");
        return -1;
    }

    if ( rc == 0 ) {
        error("sock_write: timeout");
        return -1;
    }

    if ( (pfd.revents & POLLOUT) == 0 ) {
        error("sock_read: no POLLOUT in revents");
        return 0;
    }
#else
#ifdef HAVE_SELECT
    fd_set wrevents;
    struct timeval tv;
    
    if ( FD_SETSIZE <= s ) {
        error("descriptor %d does not fit FD_SETSIZE %d", s, FD_SETSIZE);
        return -1;
    }

    FD_ZERO ( &wrevents );
    FD_SET  ( s, &wrevents );

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    while ( (rc = select( s + 1, NULL, &wrevents, NULL, &tv)) <= 0 ) {
        if ( rc == EINTR ) 
            continue;
        if ( rc == -1 ) {
            Perror("select");
            return -1;
        }
        if ( rc == 0 ) {
            errno = ETIMEDOUT;
            error("sock_write: timeout writing data");
            return -1;
        }
    }

    /*
    if ( FD_ISSET( s, &exevents ) ) {
        error("sock_write: OOB data received");
        return -1;
    }
    */
#endif
#endif    

    rc = send( s, buf, len, 0);

    return rc;
}

int sock_read( int s, void *buf, size_t len ) { 
    int rc;

#ifdef HAVE_POLL    
    struct pollfd pfd;

    int timeout = 5000; /* milliseconds */

    pfd.fd = s;
    pfd.events  = POLLIN ;
    pfd.revents = 0;

    rc = poll( &pfd, 1, timeout );

    if ( rc == -1 ) {
        Perror("sock_read: poll");
        return -1;
    }

    if ( rc == 0 ) {
        error("sock_read: timeout");
        return -1;
    }

    if ( (pfd.revents & POLLIN) == 0 ) {
        error("sock_read: no POLLIN in revents");
        return 0;
    }
#else
#ifdef HAVE_SELECT
    fd_set rdevents, exevents;
    struct timeval tv;

    if ( FD_SETSIZE <= s ) {
        error("descriptor %d does not fit FD_SETSIZE %d", s, FD_SETSIZE);
        return -1;
    }

    FD_ZERO ( &rdevents );
    FD_SET  ( s, &rdevents );

    exevents = rdevents;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    rc = select( s + 1, &rdevents, NULL, &exevents, &tv);

    if ( rc == -1 ) {
        Perror("select");
        return -1;
    }
    if ( rc == 0 ) {
        error("sock_read: timeout reading data");
        return -1;
    }
    if ( FD_ISSET( s, &exevents ) ) {
        error("sock_read: OOB data received");
        return -1;
    }
#endif
#endif    

    rc = recv( s, buf, len, 0);

    return rc;
}

typedef struct {
    int s;

    void *ibuf;
    void *obuf;

    size_t icnt;
    size_t ocnt;

    size_t ilen;
    size_t olen;

    int    itimeout; // milliseconds
    int    otimeout;

} vsock_t;

vsock_t *vsock_init(int s, size_t bufsize, int timeout ) {
    vsock_t *vsock = NULL;

    if ( bufsize <= 0 ) {
        error("vsock_init: invalid buffer size");
        return NULL;
    }

    if ( timeout < 0 ) {
        error("vsock_init: invalid timeout value");
        return NULL;
    }

    if ( (vsock = malloc(sizeof(vsock_t))) == NULL ) {
        error("vsock_init: malloc failed");
        return NULL;
    }
    bzero( vsock, sizeof(vsock_t) );

    vsock->ibuf = malloc( bufsize );
    if ( vsock->ibuf == NULL ) {
        free( vsock );
        error("vsock_init: malloc failed");
        return NULL;
    }

    vsock->obuf = malloc( bufsize );
    if ( vsock->obuf == NULL ) {
        free( vsock->ibuf );
        free( vsock );
        error("vsock_init: malloc failed");
        return NULL;
    }

    vsock->ilen = vsock->olen = bufsize;
    vsock->icnt = vsock->ocnt = 0;

    vsock->itimeout = vsock->otimeout = timeout;

    vsock->s = s;

    return vsock;
}

void vsock_free( vsock_t *vsock ) {
    assert( vsock != NULL );

    free( vsock->ibuf );
    free( vsock->obuf );
    free( vsock );
}

int vsock_fill ( vsock_t *vsock ) {
    int rc;
    rc  = sock_read( vsock->s, vsock->ibuf, vsock->ilen );
    if ( rc >= 0 )
        vsock->icnt = rc;
    return rc;
}

/* чтение строки неограниченной длины, терминированной CRLF */
/* возвращается char* выделенный malloc'ом  */

char *smtp_get( vsock_t *vsock ) {

    char *buf = NULL;
    char *eolptr = NULL;
    char *bufptr = NULL;
    char *nextptr = NULL;

    char lastchar = NULL;
    
    size_t len = 0;

    do {
        if ( vsock->icnt == 0 ) {
            if ( vsock_fill( vsock ) <= 0 ) {
                Perror("vsock_fill");
                if ( buf != NULL) 
                    free( buf );
                return NULL;
            }
        }
        if ( vsock->icnt == 0 )
            error("");

        /* XXX should truncate memory later when CRLF found ? */
        buf = realloc( buf, len + vsock->icnt);
        if ( buf ) {
            Perror("realloc");
            return NULL;
        }
        bufptr = buf + len;

        memcpy( bufptr , vsock->ibuf, vsock->icnt );

        if ( *bufptr == '\n' && lastchar == '\r' ) { 
            len += 1;
            vsock->icnt -= 1;
            buf[len] = '\0';
            return buf;
        }

        nextptr = bufptr;

        while( (eolptr = memchr( nextptr, '\r', vsock->icnt - (nextptr - bufptr) )) != NULL ) {
            if ( (size_t)(eolptr - bufptr + 1) < vsock->icnt ) {
                if ( (*(eolptr+1) == '\n') ) {
                    len += eolptr - bufptr;
                    vsock->icnt -= eolptr - bufptr;
                    buf[len] = '\0';
                    return buf;
                }
                nextptr = eolptr + 1;
            } 
            else  {
                lastchar = '\r';
                break; /* '\r' is last char in buffer */
            }
        }

        len += vsock->icnt;
        vsock->icnt = 0;

    } while( eolptr == NULL );
 
    error("never should be here");
    return NULL;
}

#if HAVE_SELECT
int isconnected( int s, fd_set *rd, fd_set *wr, fd_set *ex ) {
    int err;
    int len = sizeof( err );

    errno = 0;

    if ( !FD_ISSET( s, rd) && !FD_ISSET( s, wr) )
        return 0;
    if ( getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return 0;
    errno = err; 
    return err == 0;

}
#endif

int sock_connect( const char *addr) {

    struct sockaddr peer;
    int s;

#if HAVE_SELECT
    fd_set rdevents, wrevents, exevents;
    struct timeval tv;
    
    int flags;
    int rc;
#endif
    
    bzero( &peer,  sizeof (peer));

    if ( parse_addr( addr ) == NULL ) {
        return -1;
    }
    memcpy( &peer, parse_addr( addr ), sizeof(peer) );

    s = socket( AF_INET, SOCK_STREAM, 0);
    if ( s < 0 ) {
        Perror("socket");
        return -1;
    }

#ifdef HAVE_SELECT    
    if ( (flags = fcntl(s, F_GETFL, 0)) < 0) {
        Perror("fcntl(s, F_GETFL, 0)");
        return -1;
    }

    if ( (flags = fcntl(s, F_SETFL, O_NONBLOCK)) < 0) {
        Perror("fcntl(s, F_SETFL, O_NONBLOCK)");
        return -1;
    }

    if ( (rc = connect(s, &peer, sizeof (peer))) && errno != EINPROGRESS ) {
#else
    if ( connect(s, &peer, sizeof (peer)) ) {
#endif
        Perror("connect");
        return -1;
    }
#ifdef HAVE_SELECT
    if ( rc == 0 ) {
        if ( fcntl(s, F_SETFL, flags) < 0 ) {
            Perror("fcntl");
            return -1;
        }
        notice("connected to %s", addr );
        return s;
    }

    FD_ZERO ( &rdevents );
    FD_SET ( s, &rdevents );

    wrevents = exevents = rdevents;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    rc = select( s + 1, &rdevents, &wrevents, &exevents, &tv); 
    if ( rc < 0 ) {
        Perror("select");
        return -1;
    }
    else if ( rc == 0 ) {
        error("timeout connecting to %s", addr );
        return -1;
    }
    else if ( isconnected(s, &rdevents, &wrevents, &exevents ) ) {
        if ( fcntl(s, F_SETFL, flags) < 0 ) {
            Perror("fcntl");
            return -1;
        }
        notice("connected to %s", addr );
        return s;
    }
    else {
        error("connection to %s failed", addr );
        return -1;
    }

    
#else
    notice("connected to %s", addr );
#endif 
    return s;
}

int sock_listen( const char *addr ) {
    int s;

    const int on = 1;

    struct sockaddr_in local;

    bzero( &local, sizeof (local));
    
    if ( parse_addr( addr ) == NULL ) {
        return -1;
    }

    memcpy( &local, parse_addr( addr ), sizeof(local) );

    s = socket( AF_INET, SOCK_STREAM, 0 );

    if ( s < 0 ) {
        Perror("socket");
        return -1;
    }

    if ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) {
        Perror("setsockopt");
        return -1;
    }
    
    if ( bind( s,  (struct sockaddr *)&local, sizeof(local) )) {
        Perror("bind");
        return -1;
    }

    if ( listen( s, 1 ) ) {
        Perror("listen");
        return -1;
    }

    notice("listening on %s", addr );

    return s;
}
/*
 * TCP  host:port 
 * UNIX /path/to/socket
 * 
 */
struct sockaddr *parse_addr(const char *addr)
{
    static struct sockaddr saddr;
    struct sockaddr_in *saddr_in = (struct sockaddr_in *) & saddr;
    struct hostent *hp = NULL;

    char *ptr = NULL;
    char *a = NULL;
    char *port = NULL;

    char *protocol = "tcp";

    if (addr == NULL) {
	error("parse_addr: invalid argument");
	return NULL;
    }
    a = strdup(addr);

    bzero(&saddr, sizeof(saddr));

    if (a[0] == '/') {
	error("parse_addr: unix sockets not supported now");
	return NULL;
    }
    saddr_in->sin_family = AF_INET;

    if ((ptr = strchr(a, ':')) != NULL) {
	*ptr = '\0';
	port = strdup(ptr + 1);
    }
    if (inet_aton(a, &saddr_in->sin_addr) == 0) {
	if ((hp = gethostbyname(a)) == NULL) {
	    error("parse_addr: unknown host name %s", a);
            free( a ); 
            if ( port != NULL )
                free( port );
	    return NULL;
	}
	if (hp->h_addrtype != AF_INET) {
	    error("parse_addr: unsuppored protocol for %s (%d)", addr, hp->h_addrtype);
            free( a );
            if ( port != NULL )
                free( port );
	    return NULL;
	}
	saddr_in->sin_addr = *(struct in_addr *) hp->h_addr;
    }
    if (port == NULL) {
	error("parse_addr: unknown port %s", addr);
        free( a );
        if ( port != NULL )
            free( port );
	return NULL;
    }
    saddr_in->sin_port = htons(strtol(port, &ptr, 0));

    if (*ptr != '\0') {
	struct servent *sp;
	sp = getservbyname(port, protocol);
	if (sp == NULL) {
	    error("parse_addr: unknown service %s", port);
            free( a );
            if ( port != NULL )
                free( port );
	    return NULL;
	}
	saddr_in->sin_port = sp->s_port;
    }
    free( a );
    if ( port != NULL )
        free( port );
    return &saddr;
}
