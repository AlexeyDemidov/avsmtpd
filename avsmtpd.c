/*
 *   $Id: avsmtpd.c,v 1.2 2003-02-17 01:22:48 alexd Exp $
 *
 *   $Log: avsmtpd.c,v $
 *   Revision 1.2  2003-02-17 01:22:48  alexd
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

#include "config.h"
#include "log.h"
#include "daemon.h"
#include "sock.h"
#include "drweb.h"
#include "smtp.h"

/*
 *
 * Received: from localhost (localhost [127.0.0.1]) 
 *         by localhost with AV-filter
 *         id 18IEDH-000KuF-00; Sat, 30 Nov 2002 23:27:43 +0300
 * 
 */

char *program = PACKAGE;

const int facility = LOG_MAIL;
const char *logfile = "/tmp/avsmtpd.log";

int daemon_mode  = 1;
int debug_mode   = 0;
int verbose_mode = 0;

int run_as_suid  = 0;

const char *pid_file = "/tmp/avsmtpd.pid";
const char *euser    = "alexd";

char *connect_to = "localhost:10026";
char *bind_to    = "localhost:10025";

int   drweb_ver  = 0;
char *drweb_id   = NULL;

void av_shutdown(int rc);
void server( int s );

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
            Perror("waitpid"); 
            break; 
        } 
        notice("child process %d exited with status %d", 
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
    exit(rc);
}


void main_loop() { 
    int s, s1;

    const int on = 1;

    struct sockaddr_in local;
    struct sockaddr_in peer;

    bzero( &local, sizeof (local));
    bzero( &peer,  sizeof (local));
    
    if ( parse_addr( bind_to ) == NULL ) {
        return;
    }
    memcpy( &local, parse_addr( bind_to ), sizeof(local) );

    s = socket( AF_INET, SOCK_STREAM, 0 );

    if ( s < 0 ) {
        Perror("socket");
        return;
    }

    if ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) {
        Perror("setsockopt");
        return;
    }
    
    if ( bind( s,  (struct sockaddr *)&local, sizeof(local) )) {
        Perror("bind");
        return;
    }

    if ( listen( s, 1 ) ) {
        Perror("listen");
        return;
    }

    notice("listening on %s", bind_to );

    do {
        int peerlen = sizeof (peer);
        s1 = accept( s, (struct sockaddr *)&peer, &peerlen);

        if ( s1 < 0 ) {
            Perror("accept");
            return;
        }
        notice("client from localhost connected to %s", bind_to );
        server( s1 );
    } while(1);
}

int client_connect() {

    struct sockaddr_in peer;
    int s;
    
    bzero( &peer,  sizeof (peer));

    if ( parse_addr( connect_to ) == NULL ) {
        return -1;
    }
    memcpy( &peer, parse_addr( connect_to ), sizeof(peer) );

    s = socket( AF_INET, SOCK_STREAM, 0);

    if ( s < 0 ) {
        Perror("socket");
        return -1;
    }

    if ( connect(s, (struct sockaddr *)&peer, sizeof (peer)) ) {
        Perror("connect");
        return -1;
    }

    notice("connected to %s", connect_to );
    return s;
}

int check_data( struct mem_chunk *root ) {

    struct mem_chunk *next = root;

    void *buf, *p;
    size_t total = 0;

    while ( next != NULL && next->b != NULL ) {
        total += next->size + 2;
        next = next->next;
    }
    debug( "total data for check: %d", total );

    buf = malloc( total );
    
    if ( buf == NULL ) {
        Perror( "malloc");
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

    debug("checking data: size = %d val = <%s>", total, buf );
    return dw_scan( buf, total );
}

void server( int ssrv ) {

    struct smtp_resp *resp;
    struct smtp_cmd  *cmd;

    char *mail_from = NULL;

    int sclnt;

    if ( (sclnt = client_connect()) < 0 ) {
        error( "Can't forward connection" );

        smtp_putreply( ssrv, 554, "Sorry, can't forward connection", 0 );

        while( (cmd = smtp_readcmd(ssrv)) != NULL ) {
            if ( strcasecmp(cmd->command, "QUIT") == NULL ) 
                break;
            smtp_putreply( ssrv, 503, "bad sequence of commands", 0 );
        }
        smtp_putreply( ssrv, 221, "Bye", 0 );
        close( ssrv );

        return ;
    }

    resp = smtp_readreply( sclnt );

    if ( resp->code != 220 ) {
        error( "forward server not ready: %d %s", resp->code, resp->text  );

        while( resp != NULL && resp->cont ) {
            smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
            resp = smtp_readreply( sclnt );
        }
        
        smtp_putreply( ssrv, resp->code, resp->text, resp->cont );

        while( 1 ) {
            if ( (cmd = smtp_readcmd( ssrv )) == NULL ) {
                break;
            }
            if ( smtp_putcmd( sclnt, cmd ) ) {
                break;
            }

            resp = smtp_readreply(sclnt);
            while( resp != NULL && resp->cont ) {
                smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
                resp = smtp_readreply( sclnt );
            }
            if ( resp == NULL ) {
                break;
            }

            if ( smtp_putreply( ssrv, resp->code, resp->text, resp->cont )) {
                break;
            }
        }

        close( ssrv  );
        close( sclnt );
        
        return ;
    }

    smtp_putreply( ssrv, 220, "localhost SMTP AV-filter " VERSION, 0 );

    while( 1 ) {
        int rc;

        cmd = smtp_readcmd( ssrv );
        if ( cmd == NULL ) {
            break;
        }

        if ( strcasecmp(cmd->command, "MAIL") == NULL ) {
            mail_from = strdup(cmd->argv[0]);
        }
        
        if ( strcasecmp(cmd->command, "DATA") == NULL ) {
            struct mem_chunk *data;

            debug("smtp_putcmd: (1) %s", cmd->command);
            smtp_putreply( ssrv, 354, "End data with <CR><LF>.<CR><LF>", 0 );
            debug( "enter DATA" );
            debug("smtp_putcmd: (2) %s", cmd->command);
            data = smtp_readdata( ssrv );
            debug("smtp_putcmd: (3) %s", cmd->command);
            if ( check_data( data ) ) {
                error("message from %s infected", mail_from == NULL ?  "<unknown user>" : mail_from );
                cmd->command = "RSET";
                cmd->argc = 0;
                smtp_putcmd( sclnt, cmd );
                resp = smtp_readreply( sclnt );
                smtp_putreply(  ssrv, 550, "Content rejected (Message infected with virus)", 0 );
            }
            else { 
                notice( "message from %s passed virus check", 
                        mail_from == NULL ?  "<unknown user>" : mail_from );
                debug("smtp_putcmd: (4) %s", cmd->command);
                rc = smtp_putcmd( sclnt, cmd );
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
                    smtp_putcmd( sclnt, cmd );
                    smtp_readreply( sclnt );
                }

                smtp_printf( sclnt, "Received: from %s by %s (AV-filter %s) ;" ,
                        "localhost", "localhost", VERSION  );
                smtp_printf( sclnt, "\t%s",  mail_date( time( NULL )));

                smtp_putdata( sclnt, data );
                resp = smtp_readreply( sclnt );
                smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
            }
        } 
        else {
            rc = smtp_putcmd( sclnt, cmd );
            if ( rc ) { 
                debug( "smtp_putcmd failed, breaking loop" );
                break;
            }
            debug( "loop: rr2-1" );
            resp = smtp_readreply( sclnt );
            while( resp != NULL && resp->cont ) {
                debug( "loop: pr2-1" );
                if ( strcasecmp(resp->text, "PIPELINING") != NULL )
                    smtp_putreply(  ssrv, resp->code, resp->text, resp->cont );
                debug( "loop: rr2-2" );
                resp = smtp_readreply( sclnt );
            }
            if ( resp == NULL ) {
                debug( "smtp_readreply failed, breaking loop" );
                break;
            }
            debug( "loop: pr2-2" );
            rc = smtp_putreply( ssrv, resp->code, resp->text, resp->cont );
            if ( rc ) {
                debug( "smtp_putreply failed, breaking loop" );
                break;
            }
            if ( resp->code == 221 ) {
                debug( "221 response received" );
                break;
            }
        }

    }
    close( ssrv  );
    close( sclnt );
}

void parse_args(int argc, char **argv)
{

        int c;

        while (1) {
                c = getopt(argc, argv, "dVvc:l:p:");
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
                        case 'c': /* -c host:port */
                                connect_to = strdup(optarg);
                        case 'b': /* -b host:port */
                                bind_to = strdup(optarg);
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
    
    if (daemon_mode) { 
        if (daemon(0, 0)) 
            av_shutdown(1); 
    } 

    set_signals(); 
    signal(SIGPIPE, SIG_IGN);

    if (create_pid_file())          /* bug - don't call shutdown - it removes pidfile */ 
        av_shutdown(1);

    notice("started");

    if ( (drweb_ver = dw_getversion()) != -1 ) {
        notice("drwebd %d.%d found", drweb_ver/100, drweb_ver % 100);
    }

    if ( (drweb_id = dw_getid()) != NULL ) {
        notice("drwebd id = <%s>", drweb_id );
    }

    dw_getbaseinfo();

    main_loop();

    av_shutdown(0);
    

    return 0;
}

