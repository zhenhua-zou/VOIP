#include "calls.h"
#include "mainconfig.h"

#include <osip2/osip_mt.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

void *
os_ring_thread (void)
{
#define RING_BLOCK 160
  
  char sound_out[200];
  int fd;
  int i;
  
  fd = open(calls.ring_wav, O_RDONLY);
  
  while (calls.enable_ring != -1)
  {
    memset (sound_out, 0, RING_BLOCK);
    i = read(fd, sound_out, RING_BLOCK);
      
    if (i > 0)
      serial_out_block (cfg.serial_fd, sound_out, i);

    if ((i > 0) && (i < RING_BLOCK)) 
      lseek(fd, 0, SEEK_SET); /* return to the begining of the file */
  }

  close(fd);
  return NULL;
}


void *
os_sound_start_thread (void)
{
  char data_in[512];
  int have_more = 0;
  int timestamp = 0;
  int i;

  while (calls.enable_audio != -1)
    {
      do {
	memset (data_in, 0, 160);
	i = rtp_session_recv_with_ts (calls.rtp_session, data_in, 160,
				      timestamp, &have_more);
	if (i > 0)
	  serial_out_block (cfg.serial_fd, data_in, i);
	
      }
      while (have_more);

      timestamp += 160;
    }

  return NULL;
}

void *
os_sound_start_out_thread (void)
{
  char data_out[1000];
  int timestamp = 0;
  int i;
  int min_size = 160;

  cfg.serial_in_lock = 1;
  while (calls.enable_audio != -1)
    {
      memset (data_out, 0, min_size);
      i = serial_in_block (cfg.serial_fd, data_out, min_size);
      if (i >= 0) {
	rtp_session_send_with_ts (calls.rtp_session, data_out, i,
				  timestamp);
	timestamp = timestamp + i;
      }
    }

  cfg.serial_in_lock = 0;
  return NULL;
}

int
os_sound_init ()
{
//  FILE *log;
//  log = fopen("oRTP", "rw");
  ortp_init ();
  ortp_scheduler_init ();
//  ortp_set_log_file (log);
  return 0;
}

int
os_sound_start (int port)
{
  char localip[128];

  eXosip_guess_localip (AF_INET, localip, 128);

  if (port == 0)
    return -1;
  
  calls.rtp_session = rtp_session_new (RTP_SESSION_SENDRECV);
  
  rtp_session_set_scheduling_mode (calls.rtp_session, 1); // set scheduling mode to yes
  rtp_session_set_blocking_mode (calls.rtp_session, 1); // set block mode to yes

  rtp_session_set_profile (calls.rtp_session, &av_profile); 
  rtp_session_enable_adaptive_jitter_compensation(calls.rtp_session, 1);
  rtp_session_set_jitter_compensation (calls.rtp_session, 60);
  rtp_session_set_local_addr (calls.rtp_session, localip, port);
  rtp_session_set_remote_addr (calls.rtp_session,
                               calls.remote_sdp_audio_ip, 
			       calls.remote_sdp_audio_port);
  rtp_session_set_payload_type (calls.rtp_session, calls.payload);

  /* enter a loop (thread?) to send AUDIO data with
     rtp_session_send_with_ts(ca->rtp_session, data, data_length, timestamp);
   */
  calls.enable_audio = 1;

  calls.audio_thread = osip_thread_create(20000, os_sound_start_thread,
					calls);
  calls.out_audio_thread = osip_thread_create(20000,
					    os_sound_start_out_thread, 
					    calls);

  return 0;
}

void
os_sound_close (void)
{
  if (calls.rtp_session != NULL)
    {
      calls.enable_audio = -1;
      osip_thread_join (calls.audio_thread);
      osip_free (calls.audio_thread);
      calls.audio_thread = NULL;
      osip_thread_join (calls.out_audio_thread);
      osip_free (calls.out_audio_thread);
      calls.out_audio_thread = NULL;
      rtp_session_destroy (calls.rtp_session);
    }
  /* serial port is closed in main routine, but the data input & output is closed */
}

