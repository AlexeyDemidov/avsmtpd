#ifndef _DAEMON_H
#define _DAEMON_H

/*
 *  daemon.h - daemon mode support routines.
 *  Copyright (c) 1998 Alex L. Demidov
 *
 */

/*
 *  $Id: daemon.h,v 1.1 2003-02-16 16:44:08 alexd Exp $
 *
 *  $Log: daemon.h,v $
 *  Revision 1.1  2003-02-16 16:44:08  alexd
 *  Initial revision
 *
 *  Revision 1.3  1999/03/12 22:46:55  alexd
 *  some headers
 *
 *
 */

extern const char *euser;
extern const char *pid_file;

extern int  run_as_suid;

void sig_int ( int );
void sig_term( int );
void sig_chld( int );


int enter_suid();
int leave_suid();
int check_pid_file();
int create_pid_file();
int remove_pid_file();

#ifndef HAVE_DAEMON
int  daemon(int, int);
#endif

void set_signals();


#endif /* _DAEMON_H */
