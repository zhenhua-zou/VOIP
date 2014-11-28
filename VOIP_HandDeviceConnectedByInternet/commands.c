#include "commands.h"

extern main_config_t cfg;

int 
_check_url (char *url) 
{
  int i;
  osip_from_t *to;

  i = osip_from_init (&to);
  if (i != 0)
    return -1;
  i = osip_from_parse (to, url);
  osip_from_free (to);
  if (i != 0)
    return -1;
  return 0;
}

/* start a new call */
int 
_start_call (char *from, char *to, char *subject, 
		   char *route, char *port, void *reference)
{
  osip_message_t *invite;
  int i;
  
  osip_clrspace (to);
  osip_clrspace (subject);
  osip_clrspace (from);
  osip_clrspace (route);

  if (0 != _check_url (from))
    return -1;
  if (0 != _check_url (to))
    return -1;

  i = eXosip_call_build_initial_invite (&invite, to, from, route, subject);
  if (i != 0) {
    return -1;
  }

  /* add sdp body */
  {
    char tmp[1024];

    char localip[128];
    eXosip_guess_localip (AF_INET, localip, 128); 
    
    snprintf (tmp, 1024,	/* G726 16kbps rtpmap=98 */
              "v=0\r\n"
              "o=VoIP 0 0 IN IP4 %s\r\n"
              "s=conversation\r\n"
	      "c=IN IP4 %s\r\n"
              "t=0 0\r\n"
              "m=audio %s RTP/AVP 98\r\n"
	      "a=rtpmap:98 G726-16/8000\r\n", localip, localip, port);
//	      "a=rtpmap:0 PCMU/8000\r\n", localip, localip, port);
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_message_set_content_type (invite, "application/sdp");
  }

  eXosip_lock ();
  i = eXosip_call_send_initial_invite (invite);	/* i is the call ID */
  if (i > 0) {
    eXosip_call_set_reference (i, reference);
  }
  eXosip_unlock ();
  return i;
}

/*
 * send the register message to the server, 
 * the identity is the sip URL of the client.
 * the registrar is the sip URL of the Registrar.
 * the format is the stardard sip URL 
 * like sip:xxx@xxx.xx.xxx.xx sip:zou@172.21.134.104
 */
int
_register (char *identity, char *registrar)
{
  osip_message_t *reg = NULL;
  int i = 0;

/* check the input parameter */
  if (identity == NULL || registrar == NULL)
    return -1;

  eXosip_lock ();
  if (cfg.reg_id < 0) { 	/* has not registered yet */
    cfg.reg_id =
      eXosip_register_build_initial_register (identity, registrar, NULL,
					      120, &reg);
    /* the register time can be changed from 120s to xxx */

    if (cfg.reg_id < 0) {
      eXosip_unlock ();
      return -1;
    }
  } else {
    i = eXosip_register_build_register (cfg.reg_id, 120, &reg);
    if (i < 0) {
      eXosip_unlock ();
      return -1;
    }
  }

  eXosip_register_send_register (cfg.reg_id, reg);
  eXosip_unlock ();
  return i;
}

/* it is the same as the josua_register route
 * except that the register time is zero 
 */
int
_unregister (char *identity, char *registrar)
{
  osip_message_t *reg = NULL;
  int i = 0;

  /* check input parameter */
  if (identity == NULL || registrar == NULL)
    return -1;

  eXosip_lock ();
  if (cfg.reg_id < 0) {
    cfg.reg_id =
      eXosip_register_build_initial_register (identity, registrar, NULL, 0,
					      &reg);
    if (cfg.reg_id < 0) {
      eXosip_unlock ();
      return -1;
    }
    
  } else {
    i = eXosip_register_build_register (cfg.reg_id, 0, &reg);
    if (i < 0) {
      eXosip_unlock ();
      return -1;
    }
  }
  
  eXosip_register_send_register (cfg.reg_id, reg);
  eXosip_unlock ();
  return i;
}

 
