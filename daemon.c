/*
 *  daemon mode support routines.
 *  Copyright (c) 1998 Alex L. Demidov
 */

/*
 *  $Id: daemon.c,v 1.4 2003-02-22 18:29:57 alexd Exp $
 *
 *  $Log: daemon.c,v $
 *  Revision 1.4  2003-02-22 18:29:57  alexd
 *  added dmalloc.h, config.h
 *
 *  Revision 1.3  2003/02/17 20:42:53  alexd
 *  some more clean up
 *
 *  Revision 1.2  2003/02/17 01:22:48  alexd
 *  moved some functions to smtp.c sock.c
 *
 *  Revision 1.1.1.1  2003/02/16 16:44:08  alexd
 *  import into cvs
 *
 *  Revision 1.3  2001/03/24 17:51:05  alexd
 *  Added RCS strings to .c files
 *
 *  Revision 1.2  2000/04/23 09:19:12  alexd
 *  version 0.2
 *
 *  Revision 1.1.1.1  1999/03/12 22:41:10  alexd
 *  imported fidod
 *
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>

#include "config.h"

#ifdef HAVE_GETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "daemon.h"
#include "log.h"

#ifdef WITH_DMALLOC
#include "dmalloc.h"
#endif

#ifndef lint
static const char *rcsid = "$Id: daemon.c,v 1.4 2003-02-22 18:29:57 alexd Exp $";
static const char *revision = "$Revision: 1.4 $";
#endif


/*
 * daemon mode support
 */

int enter_suid()
{
    if ( run_as_suid == 0 )
        return 0;
    return setuid(0);
}

int leave_suid()
{
    /* FIXME check this */
    struct passwd *pwd = NULL;
    
    if ( run_as_suid == 0 )
        return 0;

    if (geteuid() != 0) {
	Perror("geteuid");
	return 1;
    }

    if ((pwd = getpwnam(euser)) == NULL) {
	Perror("getpwnam");
	return 1;
    }
    if (setgid(pwd->pw_gid) < 0) {
	Perror("setgid");
	return 1;
    }
    if (seteuid(pwd->pw_uid) < 0) {
	Perror("seteuid");
	return 1;
    }
    return 0;
}

/* returns pid number from pid file */

int check_pid_file()
{
    int fd = -1;
    int count = 0;
    char buf[32];

    debug("checking for pid file %s", pid_file);

    enter_suid();
    fd = open(pid_file, O_RDONLY);
    leave_suid();

    if (fd != -1) {

	debug("trying read pidfile %s", pid_file);

	enter_suid();
	count = read(fd, buf, sizeof(buf));
	close(fd);
	leave_suid();

	if (count > 0) {
	    pid_t pid = 0;

	    buf[count] = 0;
	    pid = atoi(buf);

	    debug("found pidfile %s pointing to process %d",
		  pid_file, pid);

	    if (pid) {
		if (pid != getpid()) {
		    if (kill(pid, 0) == 0) {
			return pid;
		    }
		    else {
			notice("Stale pidfile found %s", pid_file);
		    }
		}
	    }
	}
    }
    else {
	if (errno != ENOENT) {
	    error("unable to open pidfile for reading %s : %m", pid_file);
	    return -1;
	}
    }

    return 0;
}

int create_pid_file()
{
    int fd = -1;
    size_t count = 0;
    char buf[32];
    pid_t pid;

    pid = check_pid_file();
    if (pid) {
	if (pid != -1)
	    error("Already running with PID = %d", (int) pid);
	return 1;
    }

    remove_pid_file();

    debug("trying open pidfile %s for writing", pid_file);
    enter_suid();
    fd = open(pid_file, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0600 );
    leave_suid();

    if (fd == -1) {
	error("unable to open pidfile for writing %s : %m", pid_file);
	return 1;
    }
    snprintf(buf, sizeof(buf), "%d\n", (int) getpid());

    enter_suid();
    count = write(fd, buf, strlen(buf));
    close(fd);
    leave_suid();
    if (count != strlen(buf)) {
	error("error writing to pidfile %s:%m", pid_file);
	remove_pid_file();
    }
    snprintf(buf, sizeof(buf), "%d", (int) getpid());
    debug("pidfile %s for PID = %s created", pid_file, buf);
    return 0;
}

int remove_pid_file()
{
    int rc = 0;

    debug("trying unlink pidfile %s", pid_file);
    enter_suid();
    if (access(pid_file, F_OK) == 0)
	rc = unlink(pid_file);
    leave_suid();

    if (rc == -1) {
	error("unable to remove pidfile %s : %m", pid_file);
	return -1;
    }
    return 0;
}

#ifndef HAVE_DAEMON

int daemon(int nochdir, int noclose)
{
    pid_t pid;


    if ((pid = fork()) == -1) {
	Perror("fork");
	return -1;
    }
    else {
	if (pid)
	    _exit(0);
	else {
	    /* become process group leader */
	    if ( setsid() == -1 ) {
               Perror("setsid");
               return -1;
            } 
            
            if (!nochdir)
            	chdir("/");

	    if (!noclose) {
	        struct rlimit rlmt;
                int fd = open( "/dev/null", O_RDWR, 0);
		
                if ( fd != -1 ) { 
                  dup2(fd, 0  );
                  dup2(fd, 1 );
                  dup2(fd, 2 );
                  if ( fd > 2 )       
		    close(fd);
		  getrlimit(RLIMIT_NOFILE, &rlmt );		    
		  for ( fd = 3; fd < rlmt.rlim_max; fd++ )
		     close( fd );
                }    
	    }

	}
    }
    return 0;
}

#endif

void set_signals()
{
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_term);
    signal(SIGCHLD, sig_chld);
}
