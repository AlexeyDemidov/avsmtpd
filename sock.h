#ifndef _SOCK_H
#define _SOCK_H

/*
 *  sock.h - sockets routines.
 *  Copyright (c) 2003 Alex L. Demidov
 */

/*
 *  $Id: sock.h,v 1.2 2003-02-22 18:38:42 alexd Exp $
 *
 *  $Log: sock.h,v $
 *  Revision 1.2  2003-02-22 18:38:42  alexd
 *  add sock_listen, sock_connect, sock_write, sock_read functions
 *
 *  Revision 1.1  2003/02/17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *
 */

struct sockaddr *parse_addr  ( const char *addr );

int              sock_listen ( const char *addr );
int              sock_connect( const char *addr);

int              sock_write  ( int s, void *buf, size_t len );
int              sock_read   ( int s, void *buf, size_t len );

#endif /* _SOCK_H */
