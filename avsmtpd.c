/*
 *   $Id: avsmtpd.c,v 1.9 2003-03-01 19:52:01 alexd Exp $
 *
 *   $Log: avsmtpd.c,v $
 *   Revision 1.9  2003-03-01 19:52:01  alexd
 *   don't call drweb funcs if no_check
 *   preper address extracting from MAIL FROM:
 *
 *   Revision 1.8  2003/03/01 19:09:27  alexd
 *   fixed bug with gethostbyaddr
 *
 *   Revision 1.7  2003/03/01 12:50:57  alexd
 *   add -t switch for timeout setting
 *   make 120 sec default timeout
 *
 *   Revision 1.6  2003/02/23 12:00:28  alexd
 *   if NO_FORK don't go into daemon mode
 *
 *   Revision 1.5  2003/02/23 07:27:15  alexd
 *   change interface to vsock_
 *
 *   Revision 1.4  2003/02/22 18:37:31  alexd
 *   added dmalloc.h
 *   -n command line switch, don't check data content
 *   use sock_listen, sock_connect functions
 *   free allocated memory
 *   add NO_FORK ifdef
 *
 *   Revision 1.3  2003/02/17 01:55:37  alexd
 *   some lint cleanup
 *
 *   Revision 1.2  2003/02/17 01:22:48  alexd
 *   moved some functions to smtp.c sock.c
 *
 *
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "config.h"
#include "log.h"
#include "daemon.h"
#include "sock.h"
#include "drweb.h"
#include "smtp.h"

#ifdef WITH_DMALLOC
#include "dmalloc.h"
#endif

/*
 *
 * Received: from localhost (localhost [127.0.0.1]) 
 *         by localhost with AV-filter
 *         id 18IEDH-000KuF-00; Sat, 30 Nov 2002 23:27:43 +0300
 * 
 */

char *program = PACKAGE;

const int facility = LOG_MAIL;
const char *logfile = NULL;

int daemon_mode  = 1;
int debug_mode   = 0;
int verbose_mode = 0;
int no_check     = 0;

int run_as_suid  = 0;

const char *pid_file = "/tmp/avsmtpd.pid";
const char *euser    = "alexd";

char *connect_to = "localhost:10026";
char *bind_to    = "localhost:10025";

int   timeout    = 120000;

int   drweb_ver  = 0;
char *drweb_id   = NULL;

void av_shutdown(int rc);

void server( vsock_t *s );

void sig_int(int sig) { 
    av_shutdown(0);
}

void sig_term(int sig) { 
    av_shutdown(0);
}

void sig_chld(int sig) { 
    pid_t child; 
    int status; 
    
    while ((child = waitpid(-1, &status, WNOHANG)) != 0) { 
        if (child == -1) { 
            /* Perror("waitpid"); */
            break; 
        } 
        debug("child process %d exited with status %d", 
                child, WEXITSTATUS(status)); 
    } 
    signal(SIGCHLD, sig_chld);
}

void av_shutdown(int rc) { 
    pid_t pid; 
    
    pid = check_pid_file(); 
    if (!pid) 
        remove_pid_file(); 
    notice("shutdown"); 
    shutdownlog(); 
    free( drweb_id );
    if ( connect_to != NULL ) 
        free( connect_to );
    if ( bind_to != NULL ) 
        free( bind_to );
    exit(rc);
}


void main_loop() { 
    /* vsock_t *s;
    s = vsock_listen( bind_to, 1500, timeout );
     */

    int s;

    s = sock_listen( bind_to );

    do {
        vsock_t *ssrv;
        struct sockaddr_in peer;
        size_t peerlen = sizeof (peer);
        pid_t pid;

        struct hostent *hp;

        char *peer_addr = NULL;
        char *peer_name = NULL;
        
        bzero( &peer,  sizeof (peer));
        
        ssrv = vsock_init( accept( s, (struct sockaddr *)&peer, &peerlen), 1500, timeout );
        if ( ssrv == NULL ) {
            Perror("accept");
            return;
        }
        peer_addr = inet_ntoa(peer.sin_addr);

        hp = gethostbyaddr((char *) &peer.sin_addr, sizeof(peer.sin_addr), AF_INET);
        peer_name = (hp == NULL) ? "unknown" : hp->h_name;

        if ( hp == NULL )
            debug("server: %s", hstrerror(h_errno));

        notice("client %s[%s] connected to %s", peer_name, peer_addr, bind_to );

#ifndef NO_FORK 
        if ( (pid = fork()) == -1) {
            Perror("fork");
            /* smtp reply? */
            return ;
        }
        else {
            if ( pid ) {
                debug("child forked %d", pid);
                vsock_close( ssrv );
            }
            else {
#endif                
#ifdef HAVE_SETPROCTITLE                
                setproctitle(" server %s[%s]", peer_name, peer_addr );
#endif
                server( ssrv );
#ifndef NO_FORK
                exit(0);
            }
        }
#endif
        
    } while(1);
}

/* should return 0 on success, 1 on detected virus, -1 on error */
int check_data( struct mem_chunk *root ) {

    struct mem_chunk *next = root;

    char *buf, *p;
    size_t total = 0;
    int rc = 0;

    if ( no_check )
        return 0;

    while ( next != NULL && next->b != NULL ) {
        total += next->size + 2;
        next = next->next;
    }
    debug( "total data for check: %d", total );

    if ( total == 0 ) {
        error("check_data: zero data");
        return 0;
    }

    buf = malloc( total );
    
    if ( buf == NULL ) {
        Perror( "malloc");
        return 0;
    }
    
    p = buf;
    next = root;
    while ( next != NULL && next->b != NULL ) {
        strncpy( p, next->b, next->size );
        p += next->size;
        strncpy( p, "\r\n", 2);
        p += 2;
        next = next->next;
    }

    rc = dw_scan( buf, total );
    free( buf );
    return rc;
}

void server( vsock_t *ssrv ) {

    struct smtp_resp *resp;
    struct smtp_cmd  *cmd;

    char *mail_from = NULL;

    vsock_t *sclnt;

    if ( (sclnt = vsock_connect(connect_to, 1500, timeout)) == NULL ) {
        error( "Can't forward connection" );

        smtp_putreply( ssrv, 554, "Sorry, can't forward connection", 0 );

        while( (cmd = smtp_readcmd(ssrv)) != NULL ) {
            if ( strcasecmp(cmd->command, "QUIT") == NULL ) 
                break;
            free_smtp_cmd( cmd );
            smtp_putreply( ssrv, 503, "bad sequence of commands", 0 );
        }
        free_smtp_cmd( cmd );
        smtp_putreply( ssrv, 221, "Bye", 0 );
        vsock_close( ssrv );

        return ;
    }

    resp = smtp_readreply( sclnt );

    if ( resp->code != 220 ) {
        error( "forward server not ready: %d %s", resp->code, resp->text  );

        while( resp != NULL && resp->cont ) {
            smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
            free_smtp_resp( resp );
            resp = smtp_readreply( sclnt );
        }
        
        smtp_putreply( ssrv, resp->code, resp->text, resp->cont );
        free_smtp_resp( resp );

        while( 1 ) {
            if ( (cmd = smtp_readcmd( ssrv )) == NULL ) {
                break;
            }
            if ( smtp_putcmd( sclnt, cmd ) ) {
                free_smtp_cmd( cmd );
                break;
            }

            resp = smtp_readreply(sclnt);
            while( resp != NULL && resp->cont ) {
                smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
                free_smtp_resp( resp );
                resp = smtp_readreply( sclnt );
            }
            if ( resp == NULL ) {
                break;
            }

            if ( smtp_putreply( ssrv, resp->code, resp->text, resp->cont )) {
                free_smtp_resp( resp );
                break;
            }
            free_smtp_resp( resp );
        }

        vsock_close( ssrv  );
        vsock_close( sclnt );
        
        return ;
    } 
    free_smtp_resp( resp );

    smtp_putreply( ssrv, 220, "localhost SMTP AV-filter " VERSION, 0 );

    while( 1 ) {
        int rc;

        cmd = smtp_readcmd( ssrv );
        if ( cmd == NULL ) {
            break;
        }
        if ( !strcasecmp(cmd->command, "MAIL") ) {
            if ( mail_from != NULL ) {
                free( mail_from );
                mail_from = NULL;
            }
            if ( cmd->argv[0] != NULL) {
                char *p;
                if ( (p = strchr(cmd->argv[0], ':')) != NULL ) { 
                    mail_from = strdup( p + 1 );
                }
            }
        }
        if ( strcasecmp(cmd->command, "DATA") == NULL ) {
            struct mem_chunk *data;

            smtp_putreply( ssrv, 354, "End data with <CR><LF>.<CR><LF>", 0 );
            debug( "enter DATA" );
            data = smtp_readdata( ssrv );
            if ( check_data( data ) ) {
                free_mem_chunks( data );
                error("message from %s infected", mail_from == NULL ?  "<unknown user>" : mail_from );
                cmd->command = "RSET";
                cmd->argc = 0;
                smtp_putcmd( sclnt, cmd );
                resp = smtp_readreply( sclnt );
                free_smtp_resp( resp );
                smtp_putreply(  ssrv, 550, "Content rejected (Message infected with virus)", 0 );
            }
            else { 
                notice( "message from %s passed virus check", 
                        mail_from == NULL ?  "<unknown user>" : mail_from );

                rc = smtp_putcmd( sclnt, cmd );
                free_smtp_cmd( cmd );
                if ( rc ) { 
                    debug( "smtp_putcmd failed, breaking loop" );
                    break;
                }
                debug( "loop: rr1-1" );
                resp = smtp_readreply( sclnt );
                if ( resp == NULL ) {
                    debug( "smtp_readreply failed, breaking loop" );
                    break;
                }
                if ( resp->code != 354 ) {
                    error("DATA failed: %d %s", resp->code, resp->text);
                    cmd->command = "DATA";
                    cmd->argc = 0;
                    /* FIXME */
                    smtp_putcmd( sclnt, cmd );
                    smtp_readreply( sclnt );
                }
                free_smtp_resp( resp );

                smtp_printf( sclnt, "Received: from %s by %s (AV-filter %s) ;" ,
                        "localhost", "localhost", VERSION  );
                smtp_printf( sclnt, "\t%s",  mail_date( time( NULL )));

                smtp_putdata( sclnt, data );
                free_mem_chunks( data );
                resp = smtp_readreply( sclnt );
                smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
                free_smtp_resp( resp );
            }
        } 
        else {
            rc = smtp_putcmd( sclnt, cmd );
            free_smtp_cmd( cmd ); 
            if ( rc ) { 
                debug( "smtp_putcmd failed, breaking loop" );
                break;
            }
            resp = smtp_readreply( sclnt );
            while( resp != NULL && resp->cont ) {
#if 0                
                if ( strcasecmp(resp->text, "PIPELINING") != NULL )
#endif
                    smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
                free_smtp_resp( resp );
                resp = smtp_readreply( sclnt );
            }
            if ( resp == NULL ) {
                debug( "smtp_readreply failed, breaking loop" );
                break;
            }
            rc = smtp_putreply( ssrv, resp->code, resp->text, resp->cont );
            if ( rc ) {
                free_smtp_resp( resp );
                debug( "smtp_putreply failed, breaking loop" );
                break;
            }
            if ( resp->code == 221 ) {
                free_smtp_resp( resp );
                debug( "221 response received" );
                break;
            }
            free_smtp_resp( resp );
        }

    }
    vsock_close( ssrv  );
    vsock_close( sclnt );
}

void parse_args(int argc, char **argv)
{

        int c;

        while (1) {
                c = getopt(argc, argv, "tndVvc:f:l:p:w:b:");
                if (c == -1)
                        break;

                switch (c) {
                        case 'd':
                                debug_mode = 1;
                                verbose_mode = 1;
                                daemon_mode = 0;
                                break;
                        case 'V':
                                printf( PACKAGE " " VERSION " " __DATE__ "\n");
                                exit(0);
                                break;
                        case 'f': /* -f host:port */
                                connect_to = strdup(optarg);
                                break;
                        case 'b': /* -b host:port */
                                bind_to = strdup(optarg);
                                break;
                        case 'w': /* -w host:port */
                                drwebd_addr = strdup(optarg);
                                break;
                        case 'n':
                                no_check = 1;
                                break;
                        case 't':
                                timeout = strtol(optarg, NULL, 10);
                                break;
                        case 'v':
                                verbose_mode = 1;
                                break;
                        case 'l':                       /* FIXME: lock var from config */
                                logfile = strdup(optarg);
                                break;
                        case 'p':                       /* FIXME: lock var from config */
                                pid_file = strdup(optarg);
                                break;
                        case ':':
                                break;
                        case '?':
                                break;

                        default:
                                printf("?? getopt returned character code 0%o ??\n", c);
                }
        }

        if (optind < argc) {
                printf("non-option ARGV-elements: ");
                while (optind < argc)
                        printf("%s ", argv[optind++]);
                printf("\n");
        }

        return;
}


int main (int argc, char **argv) { 

    leave_suid();
    
    if ( (program = strrchr(argv[0], '/')) == 0 ) 
        program = argv[0]; 
    else 
        program ++;
    
    parse_args(argc, argv);

    debug("start up");

    initlog(program, facility, logfile);

    debug("init log"); 
    
#ifndef NO_FORK
    
    if (daemon_mode) { 
        if (daemon(0, 0)) 
            av_shutdown(1); 
    } 
#endif

    set_signals(); 
    signal(SIGPIPE, SIG_IGN);
    
    if (create_pid_file())          /* bug - don't call shutdown - it removes pidfile */ 
        av_shutdown(1);

    notice("started");

    if ( !no_check && (drweb_ver = dw_getversion()) != -1 ) {
        notice("drwebd %d.%d found", drweb_ver/100, drweb_ver % 100);
    }
    else {
        no_check = 1;
    }

    if ( !no_check && (drweb_id = dw_getid()) != NULL ) {
        notice("drwebd id = <%s>", drweb_id );
    }

    if ( !no_check ) {
        dw_getbaseinfo();
    }

    main_loop();

    av_shutdown(0);
    

    return 0;
}

