#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>

#include "config.h"
#include "log.h"
#include "daemon.h"
#include "drweb.h"

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

int sock_connect( struct sockaddr *addr) {
    /*
    flags = fcntl( s, F_GETFL, 0 );
    fcntl( s, F_SETFL, flags | O_NONBLOCK );

    FD_ZERO

    select( )
    */
    return -1;
}

int sock_listen( ) {
    return -1;
}

/*
 *   host:port
 *   /path/to/socket
 *
 */
struct sockaddr *parse_addr( const char *addr ) {
    static struct sockaddr saddr;

    struct sockaddr_in *saddr_in = (struct sockaddr_in *)&saddr;

    struct hostent *hp = NULL;

    char *ptr      = NULL;

    char *a        = NULL;
    char *port     = NULL;
    char *protocol = "tcp";

    if ( addr == NULL ) {
        error("parse_addr: invalid argument");
        return NULL;
    }
        
    a = strdup( addr );

    bzero( &saddr, sizeof(saddr) );

    if ( a[0] == '/') {
        error("parse_addr: unix sockets not supported now");
        return NULL;
    }
    
    saddr_in->sin_family = AF_INET;

    if ( (ptr = strchr( a, ':')) != NULL ) {
        *ptr = '\0';
        port = strdup( ptr + 1 ); 
    }

    if ( inet_aton( a, &saddr_in->sin_addr) == 0 ) {
        if ( (hp = gethostbyname(a)) == NULL ) {
            error("parse_addr: unknown host name %s", a );
            return NULL;
        }
        if ( hp->h_addrtype != AF_INET ) {
            error("parse_addr: unsuppored protocol for %s (%d)", addr, hp->h_addrtype );
            return NULL;
        }
        saddr_in->sin_addr = *(struct in_addr *)hp->h_addr;
    }

    if ( port == NULL ) {
        error("parse_addr: unknown port %s", addr );
        return NULL;
    }

    saddr_in->sin_port = htons( strtol( port, &ptr, 0 ) ); 

    if ( *ptr != '\0' ) { 
        struct servent *sp;

        sp = getservbyname( port, protocol );
        if ( sp == NULL ) {
            error("parse_addr: unknown service %s", port );
            return NULL;
        }
        saddr_in->sin_port = sp->s_port;
    }

    return &saddr;
}


void main_loop() { 
    int s, s1;

    const int on = 1;

    struct sockaddr_in local;
    struct sockaddr_in peer;

    bzero( &local, sizeof (local));
    bzero( &peer,  sizeof (local));
/*
    local.sin_family = AF_INET;
//    local.sin_addr.s_addr = htonl( INADDR_ANY );
    local.sin_port = htons( 10025 );

    inet_aton( "localhost", &local.sin_addr );
*/
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

/*    
    peer.sin_family = AF_INET;
   // peer.sin_addr.s_addr = htonl( INADDR_ANY );
    peer.sin_port = htons( 10026 );

    inet_aton( "localhost", &peer.sin_addr );
*/
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

#define DAY_MIN         (24 * HOUR_MIN) /* minutes in a day */
#define HOUR_MIN        60              /* minutes in an hour */
#define MIN_SEC         60              /* seconds in a minute */

char *mail_date ( time_t when ) { 

    static char buf[1500];
    char b2[10];
    char b3[10];

    struct tm *lt; 
    struct tm gmt; 
    int     gmtoff;

    gmt = *gmtime(&when); 
    lt = localtime(&when); 
    
    gmtoff = (lt->tm_hour - gmt.tm_hour) * HOUR_MIN + lt->tm_min - gmt.tm_min; 
    if (lt->tm_year < gmt.tm_year) 
        gmtoff -= DAY_MIN; 
    else if (lt->tm_year > gmt.tm_year) 
        gmtoff += DAY_MIN; 
    else if (lt->tm_yday < gmt.tm_yday) 
        gmtoff -= DAY_MIN; 
    else if (lt->tm_yday > gmt.tm_yday) 
        gmtoff += DAY_MIN; 
    if (lt->tm_sec <= gmt.tm_sec - MIN_SEC) 
        gmtoff -= 1; 
    else if (lt->tm_sec >= gmt.tm_sec + MIN_SEC) 
        gmtoff += 1;

    strftime( buf, sizeof(buf), "%a, %e %b %Y %H:%M:%S ", lt );
    debug("strftime. %s", buf);
    snprintf( b2, sizeof(b2), "%+03d%02d", (int) (gmtoff / HOUR_MIN),
                                       (int) (abs(gmtoff) % HOUR_MIN) );
    strftime( b3, sizeof(b3), " (%Z)", lt );

    strcat( buf , b2);
    strcat( buf , b3);

    return buf;

}

int smtp_readln( int s, char *buf, size_t len ) {

    char *buf_start = buf;
    char  c;

    static char *bp;
    static char b[1500]; /* Typical packet size */
    static int  cnt = 0; /* number of read bytes in b */

    while( len > 0 ) { /* still need more bytes */
        if ( cnt <= 0 ) { /* no data in internal buffer */

            cnt = recv( s, b, sizeof(b), 0);
            if ( cnt < 0) { /* can't read */
                if ( errno == EINTR ) 
                    continue;
                debug("recv: %s", strerror(errno) );
                return cnt;
            }
            if ( cnt == 0 ) /* no data to read */
                return 0;
            b[cnt] = '\0';
            // debug( "recv: <%s>", b);
            bp = b; /* reset head to begin of b */
        }
        c = *bp; /* next char */

        if ( *bp == '\r' && cnt > 1 && len > 1 && *(bp+1) == '\n' ) {
            *buf = '\0';
            cnt -= 2;
            bp += 2;
            return buf - buf_start;
            
        }
        *buf = c; /* store to caller buffer */
        cnt--; len--; bp++; buf++; /* */
    }
//    set_errno( EMSGSIZE );
    error("smtp_readln: MSG too big");
    return -1;
}


int smtp_putline( int s, char *b ) {
    int rc;
    size_t len;

    if ( b == NULL ) {
        error( "smtp_putline: bad argument" );
        return -1;
    }

    len = strlen( b );

    debug("smtp_putline: <%s>", b);

    rc = send( s, b, len, 0);
    if ( rc < 0 ) {
        Perror("send");
        return rc;
    }
    rc = send( s, "\r\n", 2, 0);
    if ( rc < 0 ) {
        Perror("send");
        return rc;
    }
    return rc;
}

int smtp_printf( int s, const char *fmt, ... ) {

    char b[1500];

    va_list ap;
    va_start(ap, fmt );
    

    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);

    return smtp_putline( s, b);
}

int smtp_putreply( int s, int code, const char *text, int cont ) {

    char *b;
    size_t need;

    need = strlen( text ) + 10;

    b = malloc( need );

    snprintf( b, need, "%d%c%s", code, cont ? '-' : ' '  ,text );

    debug("smtp_putreply: %s", b);
    
    return smtp_putline( s, b ) <= 0;
}


struct smtp_cmd *
smtp_readcmd( int s ) {
    static struct smtp_cmd cmd;
    
    char buf[1500];
    char *p = buf;
    char *ap = buf;
    int  i, rc;

    bzero( &cmd, sizeof(struct  smtp_cmd) );
    
    rc = smtp_readln( s, buf, sizeof( buf ) );
    if ( rc < 0) {
        return NULL;
    }

    debug("smtp_readcmd: <%s>", buf );

    cmd.argv = malloc( sizeof ( cmd.argv ) * 10);

    ap = strsep( &p , " \t");

    cmd.command = malloc( strlen(ap) );
    strcpy( cmd.command, ap );

    while( p != NULL && *p == ' ')
        p++;

    for ( i = 0;  (ap = strsep( &p, " ")) != NULL ; i++ ) {
        cmd.argv[i] = malloc( strlen(ap) );
        strcpy( cmd.argv[i], ap);

        while( p != NULL && *p == ' ')
            p++;

        cmd.argc++;

    }

    return &cmd;
}

struct smtp_resp *
smtp_readreply( int s ) {
    static struct smtp_resp resp;
    
    char buf[1500];
    char *p = buf;
    char *code_start = NULL;
    int rc;

    bzero( &resp, sizeof(struct smtp_resp));

    // debug("smtp_readreply: enter");
    rc = smtp_readln( s, buf, sizeof( buf ) );
    if ( rc < 0) {
        return NULL;
    }
    if ( rc == 0 ) {
        debug("eof");
    }
    debug("smtp_readreply: %s", buf);
    
    resp.code = 0;
    resp.text = NULL;

    while( !isdigit( *p) && (*p != NULL) ) 
        p++;

    if ( *p == 0 ) {
        error("no digits found in smtp reply");
        return NULL;
    }

    code_start = p;

    while( isdigit ( *p ) && (*p != NULL) )
        p++;

    if ( *p == '-' ) {
        resp.cont = 1;
    }
    
    if ( *p == ' ' || *p == '-' ) {
        *p = 0;
        p++;
        resp.code = atoi( code_start );
    }
    
    // debug("text_start: <%s>", p );

    resp.text = malloc(strlen(p) + 1); 
    if ( resp.text == NULL ) {
        Perror("malloc");
        return NULL;
    }
    strncpy( resp.text, p, strlen(p)  );
    resp.text[strlen(p)] = 0;

    debug( "response: code [%d] text [%s]", resp.code, resp.text);

    return &resp;
}

int smtp_putcmd( int s, struct smtp_cmd *cmd) {

    char *b;
    size_t need = 1;
    int i;

    if ( cmd == NULL || cmd->command == NULL ) {
        error("smtp_putcmd: invalid argument");
        return -1;
    }

    need += strlen( cmd->command );
    for ( i = cmd->argc ; i; ) {
        i--;
        if ( cmd->argv[i] == NULL ) {
            error("smtp_putcmd: invalid argument");
            return -1;
        }
        need += strlen(cmd->argv[i]) + 1;
    }
    b = malloc( need );
    if ( b == NULL ) {
        Perror("malloc");
    }

    strcpy( b, cmd->command );

    for ( i = 0; i < cmd->argc; i++ ) {
        strcat( b, " ");
        strcat( b, cmd->argv[i] );
    }
    debug("smtp_putcmd: <%s>", b);
    return smtp_putline( s, b ) <= 0;
}

struct mem_chunk {
    void *b;
    size_t size;
    struct mem_chunk *next;
};

struct mem_chunk *
chunk_add(  void *buf, size_t len ) {
    struct mem_chunk *chunk = NULL;

    if( (chunk = malloc(sizeof(struct mem_chunk))) == NULL ) {
        error("chunk_add: malloc failed");
        return chunk;
    }

    bzero( chunk, sizeof(struct mem_chunk) );

    if ( len == 0 ) {
        chunk->b = "";
    }
    else { 
       chunk->b = malloc( len + 1 ); // trailing \0
       chunk->size = len;
       strncpy( chunk->b, buf, len + 1); 
    }
    return chunk;
}

struct mem_chunk *smtp_readdata( int s) {
    char buf[1500];
    int rc;

    struct mem_chunk *root;
    struct mem_chunk *next;
    struct mem_chunk *prev;
        
    root = malloc( sizeof( struct mem_chunk ));
    next = root;

    while( 1 ) {
        rc = smtp_readln( s, buf, sizeof( buf ) );
        if ( rc < 0 ) {
            Perror( "smtp_readdata");
            return NULL;
        }
        debug( "smtp_readdata: %d %s", rc, buf );
        
        if ( rc == 1 && buf[0] == '.' ) {
            debug( "root->b = <%s>", root->b);
            return root;
        }

        if ( root == NULL ) {
            root = chunk_add( buf, rc );
            prev = root;
        }
        else {
            prev->next = chunk_add( buf, rc );
            prev = prev->next;
        }
        /*

        if ( rc == 0 ) {
            next->b = "";
            next->size = 0;
        }
        else {
          next->b = malloc( rc + 1 );
          next->size = rc;
          strncpy( next->b, buf, rc + 1); 
        }
        // debug( "next->b = <%s>", next->b);

        next->next = malloc( sizeof(struct  mem_chunk ));
        next = next->next;
        */
    }
    return root;
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

int smtp_putdata( int s, struct mem_chunk *root)  {
    int rc;

    if ( root == NULL ) {
        debug("root == NULL");
    }
    while ( root != NULL ) {
        if ( root->b == NULL ) {
            debug("root->b == NULL");
            break;
        }
        debug("putdate: putline = <%s>", root->b);
        rc = smtp_putline( s, root->b );
        if ( rc < 0) {
            Perror( "smtp_putdata");
            return rc;
        }
        root = root->next;
    }
    smtp_putline( s, "." );
    return 0;
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

    main_loop();

    av_shutdown(0);
    

    return 0;
}

