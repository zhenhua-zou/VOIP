#include "main.h"

/* reg_id, debug_level, port, identity, proto, to, registrar, serial_fd */
main_config_t cfg = { 
  -1, -1, -1, "\0", -1, "\0", "\0", -1, 0
 }; 

extern call_t calls;

/*
 * log the incoming event, it outputs the information in stdout 
 * about the incoming event.
 * It will be deleted in embedded systems' implementation
 */
static void
log_event (eXosip_event_t *je)
{
  char buf[100];

  buf[0] = '\0';
  if (je->type == EXOSIP_CALL_INVITE) {
    snprintf (buf, 99 , "<- (%i %i) incoming call, please answer",
	      je->cid, je->did);
  } else if (je->type == EXOSIP_CALL_ACK) {
    snprintf(buf, 99, "<- (%i %i) ACK received", je->cid, je->did);
  } else if (je->type == EXOSIP_CALL_NOANSWER) {
    snprintf (buf, 99, "<- (%i %i) No answer", je->cid, je->did);
  } else if (je->type == EXOSIP_CALL_CLOSED) {
    snprintf (buf, 99, "<- (%i %i) Call Closed", je->cid, je->did);
  } else if (je->type == EXOSIP_CALL_RELEASED) {
    snprintf (buf, 99, "<- (%i %i) Call released", je->cid, je->did);
  } else if (je->type == EXOSIP_MESSAGE_PROCEEDING
             || je->type == EXOSIP_MESSAGE_ANSWERED
	     || je->type == EXOSIP_MESSAGE_REQUESTFAILURE
             || je->type == EXOSIP_MESSAGE_SERVERFAILURE
             || je->type == EXOSIP_MESSAGE_GLOBALFAILURE
	     || je->type == EXOSIP_REGISTRATION_FAILURE) {
    if (je->response != NULL && je->request != NULL)
    {
      char *tmp = NULL;
      
      osip_to_to_str (je->request->to, &tmp);
      snprintf (buf, 99, "<- (%i) [%i %s for %s] to: %s",
		je->tid, je->response->status_code,
		je->response->reason_phrase, je->request->sip_method, tmp);
      osip_free (tmp);
    } else if (je->request != NULL) {
      snprintf (buf, 99, "<- (%i) Error for %s request",
		je->tid, je->request->sip_method);
    } else {
      snprintf (buf, 99, "<- (%i) Error for unknown request", je->tid);
    }
  } else if (je->response != NULL && je->rid > 0) {
/* registration response */
    char *tmp = NULL;
    
    osip_from_to_str (je->request->from, &tmp);
    snprintf (buf, 99, "registration <- (%i) [%i %s] from: %s",
	      je->rid, je->response->status_code,
	      je->response->reason_phrase, tmp);
    osip_free (tmp);
  } else {
    snprintf (buf, 99, "unsupported method <- (c=%i|d=%i|s=%i|n=%i|t=%i) %s",
	      je->cid, je->did, je->sid, je->nid, je->tid, je->textinfo);
  }
  printf("output: %s \n", buf);
}

int _event_get()
{
  int counter = 0;
   
  /* use events to print some info */
  eXosip_event_t *je;
  
  for(;;) {
    je = eXosip_event_wait(0, 50);
    eXosip_lock();
    
    eXosip_default_action(je);	/* initiate some default actions */
    eXosip_automatic_refresh(); /* refresh register before expiration */
    
    eXosip_unlock();
    
    if(je == NULL)
      break;
    
    counter++;
    log_event(je); 		/* log the event */

    if(je->type == EXOSIP_CALL_INVITE) {
      call_new(je);
    } else if (je->type == EXOSIP_CALL_ACK) {
      call_ack(je);
    } else if (je->type == EXOSIP_CALL_CLOSED) {
      call_closed(je);
    } else if (je->type == EXOSIP_CALL_RELEASED) {
      call_closed(je);
    } else if (je->type == EXOSIP_CALL_PROCEEDING) {
    } else if (je->type == EXOSIP_CALL_ANSWERED) {
      call_answered(je);
    } else if (je->type == EXOSIP_CALL_RINGING) {
      call_ringing(je);
    } else if (je->type == EXOSIP_CALL_REQUESTFAILURE) {
      call_failure(je);
    } else if (je->type == EXOSIP_CALL_SERVERFAILURE) {
      call_failure(je);
    } else if (je->type == EXOSIP_CALL_GLOBALFAILURE) {
      call_failure(je);
    } else if (je->type == EXOSIP_CALL_NOANSWER) {
    } else if (je->type == EXOSIP_REGISTRATION_FAILURE) {
      /*  something need to be done in this route, notified by zouzhenhua */
    } else if (je->type == EXOSIP_REGISTRATION_SUCCESS) {
      /*  something need to be done in this route, notified by zouzhenhua   */
    } else { /* answer 405 , not allowed method */
      int i;
      if (je->cid != 0) { 	/* in a call */
	osip_message_t *answer;
	eXosip_lock();
	i = eXosip_call_build_answer(je->tid, 405, &answer);
	if (i == 0)
	  eXosip_call_send_answer(je->tid, 405, answer);
	eXosip_unlock();
      } else {
	osip_message_t *answer; /* outside a call */
	eXosip_lock();
	i = eXosip_message_build_answer(je->tid, 405, &answer);
	if (i == 0) {
	  eXosip_message_send_answer(je->tid, 405, answer);
	}
	eXosip_unlock();
      }
    }
    eXosip_event_free(je);
  }
    
  if (counter > 0)
    return 0;
  return 1;
}

/* Deal with initial control messages, excluding these temp messages such as 180 or 200 */
int serial_input(int fd)
{
  unsigned char command;
  int i;

  /* There is nothing on the input */
  if (-1 == serial_in_char (fd, &command))
    return -1;

  switch (command) {
  case 0x00: /* Initial Invite from terminal */
    _start_call (cfg.identity, cfg.to, NULL, NULL, "10500", NULL);
    break;
  case 0x01: /* Emergency Invite from terminal*/
    
    /* emergence invite not implemented */
    break;
  case 0x02: /* Close Invite Calling */
    eXosip_lock ();
    i = eXosip_call_send_answer (calls.tid, 480, NULL);
    if (i == 0)
      call_remove ();
    eXosip_unlock ();
    break;
  /*  case 0x03:*/			/* Close Dialog */
    /*    
    eXosip_lock ();
    i = eXosip_call_terminate (calls.cid, calls.did);
    if (i == 0)
      call_remove ();
    eXosip_unlock ();
    break;
	*/
  case 0x04: /* Calling is established, waiting for terminal to answer */
    /* it is implemented in routine call_new() in header file calls.h  */
    break;
  case 0x05: /* Terminal has answered, dialogue starting */
    eXosip_lock();
    osip_message_t *answer = NULL;
    i = eXosip_call_build_answer (calls.tid, 200, &answer);
    if (i != 0) {
      eXosip_call_send_answer (calls.tid, 400, NULL);
    } else {
      i = sdp_complete_200ok (calls.tid, answer);
      if (i != 0) {
	osip_message_free (answer);
	eXosip_call_send_answer (calls.tid, 415, NULL);
      } else {
	eXosip_call_send_answer (calls.tid, 200, answer);
      }
    }
    eXosip_unlock();
    break;
  case 0x06: /* Terminal is error */
    break;
  case 0x07: /* Terminal is OK */
    /* do nothing here, do things in josua_event_get routine */
    break;
  default:	/* error and no input */
    printf("unsupported and no serial input!\n");
    return -1;
    break;
  }

  return 0;
  
}

int main()
{
  int i;
  
  /* initialize main configure */
  cfg.debug_level = 6;
  cfg.port = 5060;
  sprintf (cfg.identity, "sip:client@172.21.134.103");
  cfg.proto = IPPROTO_UDP;
  sprintf (cfg.to, "sip:zou@172.21.134.104:9313");
  sprintf (cfg.registrar, "sip:172.21.134.104");
  
  cfg.serial_fd = serial_init ();
  if (cfg.serial_fd < 0) {
    printf("could not initialize serial port\n");
    exit(0);
  }
    
  if (cfg.debug_level > 0) {
    printf ("josua\n");
    printf ("Debug level: %i\n",cfg.debug_level);
  }
  
  i = eXosip_init ();
  if (i != 0) {
    fprintf (stderr, "josua: could not initialize eXosip\n");
    exit (0);
  }
  
  i = eXosip_listen_addr (cfg.proto, "0.0.0.0", cfg.port, AF_INET, 0);
  if (i != 0) {
    eXosip_quit ();
    fprintf (stderr, "josua: could not initialize transport layer \n");
    exit(0);
  }

  _register (cfg.identity, cfg.registrar);

  for (;;) {
    _event_get ();
    if (cfg.serial_in_lock == 0) {
      if ( -1 != serial_input (cfg.serial_fd))
	  printf("one method input !\n");
    }
  }
  
  exit (1);
  
}
