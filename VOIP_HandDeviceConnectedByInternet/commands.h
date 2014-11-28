#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <osip2/osip_mt.h>
#include <eXosip2/eXosip.h>
#include "mainconfig.h"
#include <sys/socket.h>

int _check_url (char *url);
int _start_call (char *from, char *to, char *subject, char *route,
                       char *port, void *reference);
int _register (char *identity, char *registrar);
int _unregister (char *identity, char *registrar);

#endif
