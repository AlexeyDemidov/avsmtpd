#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "drweb.h"
#include "log.h"

/*
 * "[inet:]hostname:port"
 * "[unix]:/path/to/socket"
 */
int dw_open( const char *addr ) {
    struct sockaddr_in peer;
    int s;

    bzero( &peer,  sizeof (peer));

    peer.sin_family      = AF_INET; 
    peer.sin_port        = htons( 3000 );
    
    inet_aton( "localhost", &peer.sin_addr );

    // gethostbyname
    //
    s = socket( AF_INET, SOCK_STREAM, 0);

    if ( s < 0 ) { 
        Perror("socket"); 
        return -1;
    } 

    
    if ( connect(s, (struct sockaddr *)&peer, sizeof (peer)) ) { 
        Perror("connect"); 
        return -1;
    }

    return s;
}

int dw_close( int s) {
    return close(s);
}

int dw_write( int s, void *buf, size_t len ) {
    debug("sending %d bytes to drwebd", len);
    return (send( s, buf, len, 0 ) != len);
}

int dw_write_int( int s, int value) {
    int v = htonl( value );
    return dw_write( s, &v, sizeof(int) );
}

int dw_read_int( int s, int *value ) {
    return recv( s, value, sizeof( int ), 0);
}

int dw_init( const char *addr ) {
    return 0;
}

int dw_shutdown() {
    return 0;
}

void dw_getversion() {
}

void dw_getid() {
}

void dw_getbaseinfo() {
}

int  dw_scan( void *data, size_t len ) {

    int rc;
    int s;

    int cmd_result;

    s = dw_open( "localhost:3000" );
    if ( s < 0 ) {
        error("can't open connection to drwebd at localhost:3000");
    }

    rc = dw_write_int( s, DRWEBD_SCAN_CMD );
    if ( rc ) 
        goto error;
    
    rc = dw_write_int( s, 0 ); /* flags */
    if ( rc ) 
        goto error;

    rc = dw_write_int( s, 0);
    if ( rc ) 
        goto error;
    
    rc = dw_write_int( s, len);
    if ( rc ) 
        goto error;

    rc = dw_write( s, data, len);
    if ( rc ) 
        goto error;

    debug("write command complete");
    
    rc = dw_read_int( s, &cmd_result );

    return (ntohl(cmd_result) & DERR_VIRUS) ;

error:
    debug("can't write command to drwebd");

    dw_close( s );
    return -1;

}


#ifdef TEST
int main() {
    dw_init
}
#endif
