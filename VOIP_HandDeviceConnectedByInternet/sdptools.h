#ifndef __SDPTOOLS_H__
#define __SDPTOOLS_H__

#include "mainconfig.h"
#include <eXosip2/eXosip.h>

int sdp_complete_message (int did, sdp_message_t * remote_sdp,
                          osip_message_t * ack_or_183);
int sdp_complete_200ok (int did, osip_message_t * answer);

#endif
