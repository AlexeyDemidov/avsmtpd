#ifndef __DRWEB_H
#define __DRWEB_H

/*
 *
 *
 *
 *
 */

// Dr. Web daemon commands:
#define DRWEBD_SCAN_CMD         0x0001
#define DRWEBD_VERSION_CMD      0x0002
#define DRWEBD_BASEINFO_CMD     0x0003
#define DRWEBD_IDSTRING_CMD     0x0004

// DRWEBD_SCAN_FILE command flags:
#define DRWEBD_RETURN_VIRUSES   0x0001
#define DRWEBD_RETURN_REPORT    0x0002
#define DRWEBD_RETURN_CODES     0x0004
#define DRWEBD_HEURISTIC_ON     0x0008
#define DRWEBD_SPAM_FILTER      0x0020

/* DrWeb result codes */
#define DERR_READ_ERR           0x00001
#define DERR_WRITE_ERR          0x00002
#define DERR_NOMEMORY           0x00004
#define DERR_CRC_ERROR          0x00008
#define DERR_READSOCKET         0x00010
#define DERR_KNOWN_VIRUS        0x00020
#define DERR_UNKNOWN_VIRUS      0x00040
#define DERR_VIRUS_MODIFICATION 0x00080
#define DERR_TIMEOUT            0x00200
#define DERR_SYMLINK            0x00400
#define DERR_NO_REGFILE         0x00800
#define DERR_SKIPPED            0x01000
#define DERR_TOO_BIG            0x02000
#define DERR_TOO_COMPRESSED     0x04000
#define DERR_BAD_CALL           0x08000
#define DERR_EVAL_VERSION       0x10000
#define DERR_SPAM_MESSAGE       0x20000

#define DERR_VIRUS \
  (DERR_KNOWN_VIRUS|DERR_UNKNOWN_VIRUS|DERR_VIRUS_MODIFICATION)

int    dw_init( const char *addr );
int    dw_shutdown();

int    dw_getversion();
char  *dw_getid();
void   dw_getbaseinfo();

int    dw_scan( void *data, size_t len );

extern char *drwebd_addr;

#endif /* __DRWEB_H */
