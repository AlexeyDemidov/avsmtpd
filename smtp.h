#ifndef _SMTP_H
#define _SMTP_H

/*
 *  smtp.h - SMTP routines.
 *  Copyright (c) 2003 Alex L. Demidov
 *
 */

/*
 *  $Id: smtp.h,v 1.3 2003-02-23 07:26:33 alexd Exp $
 *
 *  $Log: smtp.h,v $
 *  Revision 1.3  2003-02-23 07:26:33  alexd
 *  change interface to vsock_
 *
 *  Revision 1.2  2003/02/22 18:39:59  alexd
 *  add dmalloc.h
 *  free malloc'ed memory after use
 *
 *  Revision 1.1  2003/02/17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *
 */

struct smtp_resp {
    int code;
    int cont;
    char *text;
};

struct smtp_cmd {
    char *command;
    int  argc;
    char **argv;
};

struct mem_chunk {
    void *b;
    size_t size;
    struct mem_chunk *next;
};

void              free_smtp_resp ( struct smtp_resp *resp );
void              free_smtp_cmd  ( struct smtp_cmd  *cmd );

void              free_mem_chunks( struct mem_chunk *root );

char             *mail_date     ( time_t when );

char             *smtp_get      (vsock_t *vsock);

int               smtp_putline  ( vsock_t *s, char *b );
int               smtp_printf   ( vsock_t *s, const char *fmt, ... );

int               smtp_putreply ( vsock_t *s, int code, const char *text, int cont );
struct smtp_resp *smtp_readreply( vsock_t *s );

struct smtp_cmd  *smtp_readcmd  ( vsock_t *s );
int               smtp_putcmd   ( vsock_t *s, struct smtp_cmd *cmd);

struct mem_chunk *smtp_readdata ( vsock_t *s);
int               smtp_putdata  ( vsock_t *s, struct mem_chunk *root);

#endif /* _SMTP_H */
