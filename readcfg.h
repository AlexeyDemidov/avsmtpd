#ifndef _READCFG_H
#define _READCFG_H

/*
 *  readcfg.h - config file support routines.
 *  Copyright (c) 1998 Alex L. Demidov
 *
 */

/*
 *  $Id: readcfg.h,v 1.1 2003-02-17 01:22:48 alexd Exp $
 *
 *  $Log: readcfg.h,v $
 *  Revision 1.1  2003-02-17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *  Revision 1.2  1999/03/12 22:46:55  alexd
 *  some headers
 *
 *
 */


extern const char *config_file;

struct _var {
    const char *name;
    void *var;
    int (*getvar)(struct _var *key, const char* str);
    int  initialized;
};

typedef struct _var var_t;
extern var_t vars[];

int getvarstr( var_t *key , const char *str);
int getvarbool( var_t *key , const char *str);

int read_config();

#endif /* _READCFG_H */
