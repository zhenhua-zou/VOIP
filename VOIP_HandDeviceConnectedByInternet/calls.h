#ifndef __CALLS_H__
#define __CALLS_H__

#define _SENDRECV 0 		/* may be used in Push To Talk multicast mode, when _RECVONLY is needed */
#define _SENDONLY 1
#define _RECVONLY 2

#include <ortp/ortp.h>
#include <eXosip2/eXosip.h>
#include "sdptools.h"
#include "serial.h"
#include "mainconfig.h"

struct call
{
  int cid;
  int did;
  int tid;

  char reason_phrase[50];
  int status_code;

  char textinfo[256];
  char req_uri[256];
  char local_uri[256];
  char remote_uri[256];
  char subject[256];

  char remote_sdp_audio_ip[50];
  int remote_sdp_audio_port;
  int payload;
  char payload_name[50];

  int enable_audio;             /* 0 started, -1 stopped */
  int local_sendrecv;           /* _SENDRECV, _SENDONLY, _RECVONLY */
  int remote_sendrecv;          /* _SENDRECV, _SENDONLY, _RECVONLY */
  char ring_wav[256];		/* ringback wav file's name, coded in ADPCM 16kbps */
  char busy_wav[256];		/* busy wav file's name, coded in ADPCM 16kbps */
  int enable_ring;		/* 0 started transmiting 180 ringing message, -1 stopped */
  
  RtpSession *rtp_session;
  struct osip_thread *audio_thread;
  struct osip_thread *out_audio_thread;
};

typedef struct call call_t;

extern call_t calls;

int os_sound_init (void);
int os_sound_start (int port);
void *os_sound_start_thread (void);
void *os_sound_start_out_thread (void);
void os_sound_close (void);
void *os_ring_thread (void);

int call_new (eXosip_event_t * je);
int call_ack (eXosip_event_t * je);
int call_answered (eXosip_event_t * je);
int call_ringing (eXosip_event_t * je);
int call_requestfailure (eXosip_event_t * je);
int call_serverfailure (eXosip_event_t * je);
int call_globalfailure (eXosip_event_t * je);
int call_closed (eXosip_event_t * je);
int call_remove (void);
int call_failure (eXosip_event_t * je);

#endif
