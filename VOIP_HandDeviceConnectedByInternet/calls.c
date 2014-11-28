#include "calls.h"

/* 本程序暂时只能支持单路的语音会话建立
 * 也即一个会话建立的整个SIP消息流都需要准确无误地完成,如果不能,则会进入一个无限长时间的等待状态
 * 如果上面的情况确实会出现,那么考虑使用定时器,为避免复杂性,暂时没有实现这个功能
 */

#define NOT_USED 0
int call_state = NOT_USED;		/* to indicate whether the call has been used, 0 means no, 1 means yes */
call_t calls;

static int
__call_init ()
{
  if (call_state != 1) { 	/* hasn't been used */
    memset(&calls, 0, sizeof(call_t));

    /* the two ringing files' name are defined here, ringback and busy */
    snprintf(calls.ring_wav, 256, "%s", "ringback.wav");
    snprintf(calls.busy_wav, 256, "%s", "busy.wav");

    if(os_sound_init() == 0)	/* initialize Ortp routines success */
      call_state = 1;		/* indicate that it is used */
  }

  return -1;
}

int
call_new (eXosip_event_t * je) 
{
  sdp_message_t *remote_sdp = NULL;

  if (call_state == 1) {	/* there is already a call */
    eXosip_call_send_answer(je->tid, 480, NULL); /* it is temporarily unavailable */
    return -1;
  } else 
    __call_init();
    
  calls.cid = je->cid;
  calls.did = je->did;
  calls.tid = je->tid;

  if (calls.did < 1 && calls.cid < 1) {
    call_remove();
    return -1;                /* not enough information for this event?? */
  }

  osip_strncpy(calls.textinfo, je->textinfo, 255); 

  /* in the case of re-INVITE message */
  if (je->response != NULL) {
    calls.status_code = je->response->status_code;
    snprintf(calls.reason_phrase, 50, je->response->reason_phrase);
  }

  /* get remote uri from the event, store in calls.remote_uri */
  if (je->request != NULL) {
    char *tmp = NULL;
    osip_from_to_str (je->request->from, &tmp);
    if (tmp != NULL) {
      snprintf (calls.remote_uri, 256, "%s", tmp);
      osip_free (tmp);
    }
  }

  /* negotiate payloads */
  if (je->request != NULL) {
    remote_sdp = eXosip_get_sdp_info(je->request);
  }

  /* get remote SDP IP address */
  if (remote_sdp == NULL) {
    eXosip_call_send_answer (calls.tid, 400, NULL);
    sdp_message_free (remote_sdp);
    call_remove ();
    return -1;
  } else {
    sdp_connection_t *conn;
    sdp_media_t *remote_med;
    char *tmp = NULL;
    
    conn = eXosip_get_audio_connection (remote_sdp);
    if (conn != NULL && conn->c_addr != NULL) {
      snprintf (calls.remote_sdp_audio_ip, 50, conn->c_addr);
    }

    /*get remote SDP IP port*/
    remote_med = eXosip_get_audio_media (remote_sdp);    
    if (remote_med == NULL || remote_med->m_port == NULL) {
      /* no audio media proposed */
      eXosip_call_send_answer (calls.tid, 415, NULL);
      sdp_message_free (remote_sdp);
      call_remove ();
      return -1;
    }
    calls.remote_sdp_audio_port = atoi (remote_med->m_port);
    
    if (calls.remote_sdp_audio_port > 0 && calls.remote_sdp_audio_ip[0] != '\0') {
      int pos;
      
      pos = 0;
      while (!osip_list_eol (&remote_med->m_payloads, pos)) {
	tmp = (char *) osip_list_get (&remote_med->m_payloads, pos);
	if (tmp != NULL &&
	    (0 == osip_strcasecmp (tmp, "98"))) { 
	  /* compared incoming payload type to our
	   * supported payload defined here, "98" is the payload type number
	   */
	  break;                                
	}
	tmp = NULL;                              
	pos++;
      }
    }
    
    if (tmp != NULL)
      calls.payload = atoi (tmp);
    else {
      eXosip_call_send_answer (calls.tid, 415, NULL);
      sdp_message_free (remote_sdp);
      call_remove ();
      return -1;
    }

  }

  serial_out_char(cfg.serial_fd, 0x80); /* output Invite to terminal */
  
  {
    unsigned char c;
    int i = 0;
    do {
      
      if (serial_in_char(cfg.serial_fd, &c) == 0) {
	
	if (c == 0x04) {
	  eXosip_lock ();
	  eXosip_call_send_answer (calls.tid, 180, NULL); 
	  /* every thing is OK, then send 180 RING message */
	  eXosip_unlock ();
	  calls.enable_ring = 1; /* enable ring */
	  osip_thread_create(20000, os_ring_thread, calls); 	/* create new pthread for ringing transmitting */ 
	  break;
	} else if (c == 0x06) {
	  eXosip_lock();
	  eXosip_call_send_answer (calls.tid, 480, NULL);
	  eXosip_unlock();
	  call_remove ();
	  break;
	}
	
      }
      i++;
      
    } while(i < 15);       		/* wait for 1.5 Second */
    
    if (i == 15) {
      eXosip_lock();
      eXosip_call_send_answer (calls.tid, 480, NULL);
      eXosip_unlock();
      call_remove();
      return -1;
    }
  }
  
  
  sdp_message_free (remote_sdp);

//  call_state = je->type;
  return 0;
}

int
call_ack (eXosip_event_t * je)
{
  if (call_state == NOT_USED 
      || calls.cid != je->cid
      || calls.did != je->did)
    return -1; /* doesn't exist this call */

  if (calls.enable_audio != 1)    // audio is not started
    {
      sdp_message_t *remote_sdp;
      sdp_message_t *local_sdp;

      remote_sdp = eXosip_get_remote_sdp (calls.did);
      local_sdp = eXosip_get_local_sdp (calls.did);
      
      if (remote_sdp != NULL && local_sdp != NULL)
        {
          sdp_connection_t *conn;
          sdp_media_t *local_med;
          sdp_media_t *remote_med;
          char *tmp = NULL;
          int audio_port = 0;

          conn = eXosip_get_audio_connection (remote_sdp);
          if (conn != NULL && conn->c_addr != NULL) {
	    snprintf (calls.remote_sdp_audio_ip, 50, conn->c_addr);
	  }
	  
          remote_med = eXosip_get_audio_media (remote_sdp);
          if (remote_med != NULL && remote_med->m_port != NULL) {
	    calls.remote_sdp_audio_port = atoi (remote_med->m_port);
	  }
	  
          local_med = eXosip_get_audio_media (local_sdp);
          if (local_med != NULL && local_med->m_port != NULL) {
	    audio_port = atoi (local_med->m_port);
	  }

          if (calls.remote_sdp_audio_port > 0
              && calls.remote_sdp_audio_ip[0] != '\0' && local_med != NULL)
	    tmp = (char *) osip_list_get (&local_med->m_payloads, 0);
	  
          if (tmp != NULL)           
	    calls.payload = atoi (tmp);
	  
          if (tmp != NULL
              && audio_port > 0
              && calls.remote_sdp_audio_port > 0
              && calls.remote_sdp_audio_ip[0] != '\0')

            {
              /* set to sendrecv mode default */
	      calls.local_sendrecv = _SENDRECV;
	      calls.remote_sendrecv = _SENDRECV;

              if (0 == os_sound_start (audio_port))
		calls.enable_audio = 1; /* audio is started */
            }
        }
      sdp_message_free (local_sdp);
      sdp_message_free (remote_sdp);
    }

  call_state = je->type;
  return 0;
}

int
call_ringing (eXosip_event_t * je)
{
  if (call_state == 1) {	/* there is already a call */
    eXosip_call_send_answer(je->tid, 480, NULL); /* it is temporarily unavailable */
    return -1;
  } else 
    __call_init();
  
  calls.cid = je->cid;
  calls.did = je->did;
  
  if (calls.did < 1 || calls.cid < 1) {
    call_remove ();
    return -1;            /* not enough information for this event?? */
  }
  
  calls.tid = je->tid;
  
  osip_strncpy (calls.textinfo, je->textinfo, 255);

  if (je->response != NULL) {
    calls.status_code = je->response->status_code;
    snprintf (calls.reason_phrase, 50, je->response->reason_phrase);
  }

  if (je->request != NULL) {
    char *tmp = NULL;
    osip_from_to_str (je->request->from, &tmp);
    if (tmp != NULL) {
      snprintf (calls.remote_uri, 256, "%s", tmp);
      osip_free (tmp);
    }
  }

  if(0) {
  if (calls.enable_audio == 1 && je->response != NULL)
  {
    sdp_message_t *sdp = eXosip_get_sdp_info (je->response);

    if (sdp != NULL) {
      /* audio is started and session may just have been modified */
      sdp_message_free (sdp);
    }
  }

  if (calls.enable_audio != 1)    /* audio is not started */                
  {
    sdp_message_t *remote_sdp;
    sdp_message_t *local_sdp;

    local_sdp = eXosip_get_sdp_info (je->request);
    remote_sdp = eXosip_get_sdp_info (je->response);

    if (remote_sdp != NULL && local_sdp != NULL)
    {
      sdp_connection_t *conn;
      sdp_media_t *local_med;
      sdp_media_t *remote_med;
      char *tmp = NULL;
      int audio_port = 0;

      conn = eXosip_get_audio_connection (remote_sdp);
      if (conn != NULL && conn->c_addr != NULL)
	snprintf (calls.remote_sdp_audio_ip, 50, conn->c_addr);
      
      remote_med = eXosip_get_audio_media (remote_sdp);
      if (remote_med != NULL && remote_med->m_port != NULL) 
	calls.remote_sdp_audio_port = atoi (remote_med->m_port);
      
      local_med = eXosip_get_audio_media (local_sdp);
      if (local_med != NULL && local_med->m_port != NULL) 
	audio_port = atoi (local_med->m_port);
      
	  
      if (calls.remote_sdp_audio_port > 0
	  && calls.remote_sdp_audio_ip[0] != '\0' && remote_med != NULL)
	tmp = (char *) osip_list_get (&remote_med->m_payloads, 0);
      
      if (tmp != NULL) 
	calls.payload = atoi (tmp);
      
      if (tmp != NULL
	  && audio_port > 0
	  && calls.remote_sdp_audio_port > 0
	  && calls.remote_sdp_audio_ip[0] != '\0') {
	/* search if stream is sendonly or recvonly */
	calls.remote_sendrecv = _SENDRECV;
	calls.local_sendrecv = _SENDRECV;
	if (0 == os_sound_start (audio_port)) {
	  calls.enable_audio = 1; /* audio is started */
	}
      }
    }
    sdp_message_free (local_sdp);
    sdp_message_free (remote_sdp);
  }
  } /* end of if(0) */
  
  serial_out_char(cfg.serial_fd, 0x82);	/* waiting for answering */

  /* output ringing wav */
  osip_thread_create(20000, os_ring_thread, calls); /* create new pthread for ringing transmitting */
  
  call_state = je->type;;
  return 0;
}

int
call_answered (eXosip_event_t * je)
{
  if (call_state == NOT_USED
      || calls.cid != je->cid || calls.did != je->did)
    return -1;
  
  if (calls.did < 1 && calls.cid < 1) {
    call_remove ();
    return -1;            /* not enough information for this event?? */
  }
      
  osip_strncpy (calls.textinfo, je->textinfo, 255);

  if (je->response != NULL) {
    calls.status_code = je->response->status_code;
    snprintf (calls.reason_phrase, 50, je->response->reason_phrase);
  }

  if (je->request != NULL) {
    char *tmp = NULL;
    
    osip_from_to_str (je->request->from, &tmp);
    if (tmp != NULL) {
      snprintf (calls.remote_uri, 256, "%s", tmp);
      osip_free (tmp);
    }
  }
  eXosip_lock ();
  {
    osip_message_t *ack = NULL;
    int i;

    i = eXosip_call_build_ack (calls.did, &ack);
    if (i != 0)
      {
    } else
      {
        sdp_message_t *local_sdp = NULL;
        sdp_message_t *remote_sdp = NULL;

        if (je->request != NULL && je->response != NULL) {
	  local_sdp = eXosip_get_sdp_info (je->request);
	  remote_sdp = eXosip_get_sdp_info (je->response);
	}
        if (local_sdp == NULL && remote_sdp != NULL)/* sdp in ACK */
	  i = sdp_complete_message (calls.did, remote_sdp, ack);
        sdp_message_free (local_sdp);
        sdp_message_free (remote_sdp);
	
        eXosip_call_send_ack (calls.did, ack);
	
	serial_out_char(cfg.serial_fd, 0x83); /* output 0x83 message */
	calls.enable_ring = -1; /* stop ring thread */
      }
  }
  eXosip_unlock ();
  
  if (calls.enable_audio == 1 && je->response != NULL)
    {
      sdp_message_t *sdp = eXosip_get_sdp_info (je->response);

      if (sdp != NULL)
        {
          /* audio is started and session has just been modified */
          calls.enable_audio = -1;
          os_sound_close ();
          sdp_message_free (sdp);
        }
    }

  if (calls.enable_audio != 1)    /* audio is not started */
    {
      sdp_message_t *remote_sdp;
      sdp_message_t *local_sdp;

      local_sdp = eXosip_get_sdp_info (je->request);
      remote_sdp = eXosip_get_sdp_info (je->response);

      if (remote_sdp != NULL && local_sdp != NULL)
        {
          sdp_connection_t *conn;
          sdp_media_t *local_med;
          sdp_media_t *remote_med;
          char *tmp = NULL;
          int audio_port = 0;

          conn = eXosip_get_audio_connection (remote_sdp);
          if (conn != NULL && conn->c_addr != NULL) {
	    snprintf (calls.remote_sdp_audio_ip, 50, conn->c_addr);
	  }
          remote_med = eXosip_get_audio_media (remote_sdp);
          if (remote_med != NULL && remote_med->m_port != NULL) {
	    calls.remote_sdp_audio_port = atoi (remote_med->m_port);
	  }
          local_med = eXosip_get_audio_media (local_sdp);
          if (local_med != NULL && local_med->m_port != NULL) {

	    audio_port = atoi (local_med->m_port);
	  }
	  
          if (calls.remote_sdp_audio_port > 0
              && calls.remote_sdp_audio_ip[0] != '\0' && remote_med != NULL)
	    tmp = (char *) osip_list_get (&remote_med->m_payloads, 0);

          if (tmp != NULL)
	    calls.payload = atoi (tmp);
          if (tmp != NULL
              && audio_port > 0
              && calls.remote_sdp_audio_port > 0
              && calls.remote_sdp_audio_ip[0] != '\0')
	  {
	    calls.remote_sendrecv = _SENDRECV;
	    calls.local_sendrecv = _SENDRECV;
	    if (0 == os_sound_start (audio_port)) {
	      calls.enable_audio = 1; /* audio is started */
	    }
	  }
	  
        }
      sdp_message_free (local_sdp);
      sdp_message_free (remote_sdp);
    }

  call_state = je->type;
  return 0;
}

int
call_failure (eXosip_event_t * je)
{
  if (call_state == NOT_USED
      || calls.cid != je->cid || calls.did != je->did)
    return -1;
  
  if ((je->response != NULL && je->response->status_code == 407)
      || (je->response != NULL && je->response->status_code == 401))/* try authentication */
    return 0;
  
  if (calls.enable_audio > 0) {
    calls.enable_audio = -1;
    os_sound_close ();
  }
  
  if (je->response != NULL) {
    calls.status_code = je->response->status_code;
    snprintf (calls.reason_phrase, 50, je->response->reason_phrase);
  }
  
  call_state = NOT_USED;

  serial_out_char(cfg.serial_fd, 0x84); /* output error relevant to network */
  calls.enable_ring = -1; 	/* close ring */
  
  return 0;
}

int
call_closed (eXosip_event_t * je)
{
  if(call_state == NOT_USED
     || calls.cid != je->cid)
    return -1;

  if (calls.enable_audio > 0) {
    calls.enable_audio = -1;
    os_sound_close ();
  }

  calls.enable_ring = -1;
  call_state = NOT_USED;
         
  serial_out_char(cfg.serial_fd, 0x81);
  
  return 0;
}

int
call_remove (void)
{
  if (calls.enable_audio > 0) {
    calls.enable_audio = -1;
    os_sound_close ();
  }

  call_state = NOT_USED;
  calls.enable_ring = -1;
  return 0;
}

