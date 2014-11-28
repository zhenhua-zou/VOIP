#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <term.h>

#include <osip2/osip_mt.h>
#include <eXosip2/eXosip.h>
#include "calls.h"
#include "commands.h"
#include "serial.h"
#include "mainconfig.h"

void log_event(eXosip_event_t *je);
int _event_get(void);
int serial_input(int fd);

#endif
