/*
 * $Id: sock.c,v 1.8 2003-03-01 19:49:33 alexd Exp $
 * 
 * $Log: sock.c,v $
 * Revision 1.8  2003-03-01 19:49:33  alexd
 * replace notice(connected) with similar debug()
 *
 * Revision 1.7  2003/03/01 19:17:07  alexd
 * fix double calls to parse_addr
 *
 * Revision 1.6  2003/02/23 15:50:17  alexd
 * fix bug, where lastchar in buffer = '\r'
 *
 * Revision 1.5  2003/02/23 11:59:33  alexd
 * added vsock_write, vsock_{i|o}flush
 *
 * Revision 1.4  2003/02/23 07:26:08  alexd
 * added vsock_ functions
 * make timeout configurable
 *
 * Revision 1.3  2003/02/22 18:38:42  alexd
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
#include <fcntl.h>

#include "config.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef WITH_DMALLOC
#include "dmalloc.h"
#endif


#include "log.h"
#include "sock.h"

int sock_write( int s, void *buf, size_t len, int timeout ) { 
    int rc;

#ifdef HAVE_POLL    
    struct pollfd pfd;

    pfd.fd = s;
    pfd.events  = POLLOUT ;
    pfd.revents = 0;

    rc = poll( &pfd, 1, timeout );

    if ( rc == -1 ) {
        Perror("sock_write: poll");
        return -1;
    }

    if ( rc == 0 ) {
        errno = ETIMEDOUT;
        error("sock_write: timeout");
        return -1;
    }

    if ( (pfd.revents & POLLOUT) == 0 ) {
        error("sock_write: no POLLOUT in revents");
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

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec =  (timeout % 1000) * 1000;

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

int sock_read( int s, void *buf, size_t len, int timeout ) { 
    int rc;

#ifdef HAVE_POLL    
    struct pollfd pfd;

    pfd.fd = s;
    pfd.events  = POLLIN ;
    pfd.revents = 0;

    rc = poll( &pfd, 1, timeout );

    if ( rc == -1 ) {
        Perror("sock_read: poll");
        return -1;
    }

    if ( rc == 0 ) {
        errno = ETIMEDOUT;
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

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec =  (timeout % 1000) * 1000;

    rc = select( s + 1, &rdevents, NULL, &exevents, &tv);

    if ( rc == -1 ) {
        Perror("select");
        return -1;
    }
    if ( rc == 0 ) {
        error("sock_read: timeout reading data");
        errno = ETIMEDOUT;
        return -1;
    }
    if ( FD_ISSET( s, &exevents ) ) {
        error("sock_read: OOB data received");
        return -1;
    }
#endif
#endif    

    rc = recv( s, buf, len , 0);
    return rc;
}

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
    
    vsock->ilen = vsock->olen = bufsize;

    vsock->ibuf = malloc( bufsize + 1);
    if ( vsock->ibuf == NULL ) {
        free( vsock );
        error("vsock_init: malloc failed");
        return NULL;
    }
    bzero( vsock->ibuf, bufsize + 1 );
    debug("allocated ibuf size = %d, %d", bufsize, vsock->ilen );

    vsock->obuf = malloc( bufsize + 1);
    if ( vsock->obuf == NULL ) {
        free( vsock->ibuf );
        free( vsock );
        error("vsock_init: malloc failed");
        return NULL;
    }
    bzero( vsock->obuf, bufsize + 1 );
    debug("allocated obuf size = %d, %d", bufsize, vsock->olen );

    vsock->icnt = vsock->ocnt = 0;
    vsock->iptr = vsock->ibuf;
    vsock->optr = vsock->obuf;

    vsock->itimeout = vsock->otimeout = timeout;

    vsock->s = s;

    return vsock;
}

vsock_t *vsock_listen( const char *addr, size_t bufsize, int timeout ) {
    int s = 0;

    s = sock_listen( addr );

    if ( s < 0 ) 
        return NULL;

    return vsock_init(s, bufsize, timeout );
}

vsock_t *vsock_connect( const char *addr, size_t bufsize, int timeout ) {
    int s = 0;

    s = sock_connect( addr );

    if ( s < 0 ) 
        return NULL;

    return vsock_init(s, bufsize, timeout );
}

int vsock_close( vsock_t *vsock ) {
    int s = vsock->s;
    vsock_free( vsock );
    return close(s);
}

void vsock_free( vsock_t *vsock ) {
    assert( vsock != NULL );

    free( vsock->ibuf );
    free( vsock->obuf );
    free( vsock );
}

int vsock_write( vsock_t *vsock, void *buf, size_t len ) {
    int rc = 0;

    assert( len > 0 );
    assert( buf   != NULL );
    assert( vsock != NULL );

    while( len > 0 ) {
        size_t to_copy;

        /* no buffer space, flush */
        if ( vsock->ocnt + len > vsock->olen ) {
            rc = vsock_oflush( vsock );
            if ( rc < 0 ) {
                error("vsock_write: error writing output");
                return rc;
            }
        }
        
        /* copy to buffer no more then buffer size bytes  */
        to_copy = vsock->olen > len ? len : vsock->olen;
        memcpy( vsock->optr, buf, to_copy  );
        /*
        debug("vsock_write: coping %d bytes to buffer: %s", to_copy, buf);
        */
        
        vsock->optr += to_copy;
        vsock->ocnt += to_copy;

        len -= to_copy;
        rc += to_copy;
    }
    debug("vsock_write: writen %d bytes", rc);
    return rc;
}

int vsock_oflush( vsock_t *vsock ) {
    int rc = 0;

    if ( vsock->ocnt > 0 ) {
        /*
        debug("vsock_oflush: output buffer size = %d, val = %s", vsock->ocnt, vsock->obuf );
        */
        rc = sock_write( vsock->s, vsock->obuf, vsock->ocnt, vsock->otimeout );
        vsock->optr = vsock->obuf;
        vsock->ocnt = 0;
        debug("vsock_oflush: wirten %d bytes", rc);
    }
    return rc;
}

int vsock_iflush( vsock_t *vsock ) {
    if ( vsock->icnt > 0 ) {
        vsock->icnt = 0;
        vsock->iptr = vsock->ibuf;
    }
    return 0;
}

int vsock_fill ( vsock_t *vsock ) {
    int rc;

    if ( vsock == NULL ) {
        error("vsock_fill: bad argument");
        return -1;
    }
    if ( vsock->ibuf == NULL ) {
        error("vsock_fill: buffer not allocated");
        return -1;
    }

    rc  = sock_read( vsock->s, vsock->ibuf, vsock->ilen, vsock->itimeout );

    vsock->iptr = vsock->ibuf;

    if ( rc >= 0 )
        vsock->icnt = rc;
    else 
        vsock->icnt = 0;
    debug("fill buffer with %d chars ", vsock->icnt);
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
        if ( buf == NULL ) {
            debug("buf = malloc(%d)", len + vsock->icnt + 1 );
            buf = malloc( len + vsock->icnt + 1);
        }
        else { 
            debug("buf = realloc(%d)", len + vsock->icnt + 1 );
            buf = realloc( buf, len + vsock->icnt + 1);
        }

        if ( buf == NULL ) {
            Perror("realloc");
            return NULL;
        }
        bufptr = buf + len;

        memcpy( bufptr , vsock->iptr, vsock->icnt );

        if ( *bufptr == '\n' && lastchar == '\r' ) { 
            debug("lastchar was '\\r' and next char = '\\n'");
            len += 2;
            vsock->icnt -= 1;
            vsock->iptr += 1;
            buf[len] = '\0';
            return buf;
        }

        nextptr = bufptr;

        while( (eolptr = memchr( nextptr, '\r', vsock->icnt - (nextptr - bufptr) )) != NULL ) {
            debug("found '\\r' symbol");
            if ( ((size_t)(eolptr - bufptr + 1)) < vsock->icnt ) {
                if ( (*(eolptr+1) == '\n') ) {
                    debug("found '\\n' symbol");
                    len += eolptr - bufptr;
                    debug("read %d chars", eolptr - bufptr + 2);
                    vsock->icnt -= eolptr - bufptr + 2;
                    debug("%d chars left in buffer", vsock->icnt );
                    buf = realloc( buf, len + 1);
                    debug("result string have %d chars", len);
                    buf[len] = '\0';
                    vsock->iptr += eolptr - bufptr + 2;
                    debug("all ok, ret ahead");
                    return buf;
                }
                nextptr = eolptr + 1;
            } 
            else  {
                debug("last char is '\\r'");
                eolptr = NULL;
                lastchar = '\r'; /* found CR, but it last char in buf*/
                break;           /* should check next buffer */
            }
        }

        /* no CR LF found */
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

    struct sockaddr   peer;
    struct sockaddr *ppeer;
    int s;

#if HAVE_SELECT
    fd_set rdevents, wrevents, exevents;
    struct timeval tv;
    
    int flags;
    int rc;
#endif
    
    bzero( &peer,  sizeof (peer));

    if ( (ppeer = parse_addr( addr )) == NULL ) {
        return -1;
    }
    memcpy( &peer, ppeer, sizeof(peer) );

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
        debug("connected to %s", addr );
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
        debug("connected to %s", addr );
        return s;
    }
    else {
        error("connection to %s failed", addr );
        return -1;
    }

    
#else
    debug("connected to %s", addr );
#endif 
    return s;
}

int sock_listen( const char *addr ) {
    int s;

    const int on = 1;

    struct sockaddr_in   local;
    struct sockaddr    *plocal;

    bzero( &local, sizeof (local));
    
    if ( (plocal = parse_addr(addr)) == NULL ) {
        return -1;
    }

    memcpy( &local, plocal, sizeof(local) );

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

struct sockaddr * parse_addr(const char *addr)
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
