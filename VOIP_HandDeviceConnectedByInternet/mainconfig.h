#ifndef _MAINCONFIG_H_
#define _MAINCONFIG_H_ 1

typedef struct _main_config_t
{
  int reg_id;			/* registration ID number */
  int debug_level;              /* <verbose level> */
  int port;                     /* <SIP port>  default is 5060 */
  char identity[256];           /* <from url>  local identity */
  int proto;                    /* 0 (udp) 1 (tcp) 2 (tls) */
  char to[256];			/* the Callee sip URL */
  char registrar[256]; 		/* the Registrar sip URL */
  int serial_fd;		/* serial port file descriptor */
  int serial_in_lock;
  
} main_config_t;

extern main_config_t cfg;

#endif
