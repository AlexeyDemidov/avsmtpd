#ifndef _SOCK_H
#define _SOCK_H

/*
 *  sock.h - sockets routines.
 *  Copyright (c) 2003 Alex L. Demidov
 */

/*
 *  $Id: sock.h,v 1.4 2003-02-23 11:59:33 alexd Exp $
 *
 *  $Log: sock.h,v $
 *  Revision 1.4  2003-02-23 11:59:33  alexd
 *  added vsock_write, vsock_{i|o}flush
 *
 *  Revision 1.3  2003/02/23 07:25:31  alexd
 *  added vsock_ functions
 *
 *  Revision 1.2  2003/02/22 18:38:42  alexd
 *  add sock_listen, sock_connect, sock_write, sock_read functions
 *
 *  Revision 1.1  2003/02/17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *
 */

typedef struct {
    int s;

    void *ibuf;
    void *obuf;
    
    void *iptr;
    void *optr;

    size_t icnt;
    size_t ocnt;

    size_t ilen;
    size_t olen;

    int    itimeout; // milliseconds
    int    otimeout;

} vsock_t;

struct sockaddr *parse_addr  ( const char *addr );

vsock_t         *vsock_init   ( int s, size_t bufsize, int timeout );
vsock_t         *vsock_connect( const char *addr, size_t bufsize, int timeout );
vsock_t         *vsock_listen ( const char *addr, size_t bufsize, int timeout );

int              vsock_close  ( vsock_t *vsock );
void             vsock_free   ( vsock_t *vsock );

int              vsock_write  ( vsock_t *s, void *buf, size_t len );
int              vsock_read   ( vsock_t *s, void *buf, size_t len );
int              vsock_oflush ( vsock_t *vsock );
int              vsock_iflush ( vsock_t *vsock );

int              sock_listen ( const char *addr );
int              sock_connect( const char *addr);

int              sock_write  ( int s, void *buf, size_t len, int timeout );
int              sock_read   ( int s, void *buf, size_t len, int timeout );

#endif /* _SOCK_H */
