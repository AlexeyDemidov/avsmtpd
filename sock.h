#ifndef _SOCK_H
#define _SOCK_H

/*
 *  sock.h - sockets routines.
 *  Copyright (c) 2003 Alex L. Demidov
 */

/*
 *  $Id: sock.h,v 1.1 2003-02-17 01:22:48 alexd Exp $
 *
 *  $Log: sock.h,v $
 *  Revision 1.1  2003-02-17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *
 */


struct sockaddr *parse_addr( const char *addr );

#endif /* _SOCK_H */
