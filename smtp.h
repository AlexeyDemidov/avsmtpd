#ifndef _SMTP_H
#define _SMTP_H

/*
 *  smtp.h - SMTP routines.
 *  Copyright (c) 2003 Alex L. Demidov
 *
 */

/*
 *  $Id: smtp.h,v 1.1 2003-02-17 01:22:48 alexd Exp $
 *
 *  $Log: smtp.h,v $
 *  Revision 1.1  2003-02-17 01:22:48  alexd
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

char             *mail_date ( time_t when );

int               smtp_readln( int s, char *buf, size_t len );
int               smtp_putline( int s, char *b );
int               smtp_printf( int s, const char *fmt, ... );

int               smtp_putreply( int s, int code, const char *text, int cont );
struct smtp_resp *smtp_readreply( int s );

struct smtp_cmd  *smtp_readcmd( int s );
int               smtp_putcmd( int s, struct smtp_cmd *cmd);

struct mem_chunk *smtp_readdata( int s);
int               smtp_putdata( int s, struct mem_chunk *root);

#endif /* _SMTP_H */
