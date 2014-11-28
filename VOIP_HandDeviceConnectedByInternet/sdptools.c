#include <sys/socket.h>
#include <osip2/osip_mt.h>
#include "sdptools.h"
#include "calls.h"

extern call_t calls;

int
sdp_complete_message (int did, sdp_message_t * remote_sdp, 
		      osip_message_t * msg)
{
  sdp_media_t *remote_med;
  char *tmp = NULL;
  char buf[4096];
  int pos;
  char localip[128];

  if (remote_sdp == NULL) {
    return -1;
  }
  if (msg == NULL) {
    return -1;
  }

  eXosip_guess_localip (AF_INET, localip, 128);
  snprintf (buf, 4096,
            "v=0\r\n"
            "o=josua 0 0 IN IP4 %s\r\n"
            "s=conversation\r\n" "c=IN IP4 %s\r\n" "t=0 0\r\n", 
	    localip, localip);

  pos = 0;
  while (!osip_list_eol (&remote_sdp->m_medias, pos))
  {
    char payloads[128];
    int pos2;

    memset (payloads, '\0', sizeof (payloads));
    remote_med = (sdp_media_t *) osip_list_get (&remote_sdp->m_medias, pos);

    if (0 == osip_strcasecmp (remote_med->m_media, "audio"))
    {
      pos2 = 0;
      while (!osip_list_eol (&remote_med->m_payloads, pos2))
      {
	tmp = (char *) osip_list_get (&remote_med->m_payloads, pos2);
	if (tmp != NULL &&
	    (0 == osip_strcasecmp (tmp, "0")
	     || 0 == osip_strcasecmp (tmp, "8"))) {
	  strcat (payloads, tmp);
	  strcat (payloads, " ");
	}
	pos2++;
      }
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      if (pos2 == 0 || payloads[0] == '\0') {
	strcat (buf, " 0 RTP/AVP \r\n");
	return -1;        /* refuse anyway */
      } else {
	strcat (buf, " 10500 RTP/AVP ");
	strcat (buf, payloads);
	strcat (buf, "\r\n");

	/* add the ADPCM payload code zouzhenhua */
	if (NULL != strstr (payloads, " 0 ")
	    || (payloads[0] == '0' && payloads[1] == ' '))
	  strcat (buf, "a=rtpmap:0 PCMU/8000\r\n");
	if (NULL != strstr (payloads, " 8 ")
	    || (payloads[0] == '8' && payloads[1] == ' '))
	  strcat (buf, "a=rtpmap:8 PCMA/8000\r\n");
      }
    } else {
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      strcat (buf, " 0 ");
      strcat (buf, remote_med->m_proto);
      strcat (buf, " \r\n");
    }
    pos++;
  }

  osip_message_set_body (msg, buf, strlen (buf));
  osip_message_set_content_type (msg, "application/sdp");
  return 0;
}

int
sdp_complete_200ok (int did, osip_message_t * answer)
{
  sdp_message_t *remote_sdp;
  sdp_media_t *remote_med;
  char *tmp = NULL;
  char buf[4096];
  int pos;

  char localip[128];

  remote_sdp = eXosip_get_remote_sdp (did);
  if (remote_sdp == NULL) {
    return -1;                /* no existing body? */
  }

  eXosip_guess_localip (AF_INET, localip, 128);
  snprintf (buf, 4096,
            "v=0\r\n"
            "o=josua 0 0 IN IP4 %s\r\n"
            "s=conversation\r\n" "c=IN IP4 %s\r\n" "t=0 0\r\n", 
	    localip, localip);

  pos = 0;
  while (!osip_list_eol (&remote_sdp->m_medias, pos))
  {
    char payloads[128];
    int pos2;

    memset (payloads, '\0', sizeof (payloads));
    remote_med = (sdp_media_t *) osip_list_get (&remote_sdp->m_medias, pos);

    if (0 == osip_strcasecmp (remote_med->m_media, "audio"))
    {
      pos2 = 0;
      while (!osip_list_eol (&remote_med->m_payloads, pos2))
      {				/* add the ADPCM payload type, zouzhenhua */
	tmp = (char *) osip_list_get (&remote_med->m_payloads, pos2);
	if (tmp != NULL &&
	    (0 == osip_strcasecmp (tmp, "0")
	     || 0 == osip_strcasecmp (tmp, "8"))) {
	  strcat (payloads, tmp);
	  strcat (payloads, " ");
	}
	pos2++;
      }
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      if (pos2 == 0 || payloads[0] == '\0') {
	strcat (buf, " 0 RTP/AVP \r\n");
	sdp_message_free (remote_sdp);
	return -1;        /* refuse anyway */
      } else {
	strcat (buf, " 10500 RTP/AVP ");
	strcat (buf, payloads);
	strcat (buf, "\r\n");

	if (NULL != strstr (payloads, " 0 ")
	    || (payloads[0] == '0' && payloads[1] == ' '))
	  strcat (buf, "a=rtpmap:0 PCMU/8000\r\n");
	if (NULL != strstr (payloads, " 8 ")
	    || (payloads[0] == '8' && payloads[1] == ' '))
	  strcat (buf, "a=rtpmap:8 PCMA/8000\r\n");
      }
    } else {
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      strcat (buf, " 0 ");
      strcat (buf, remote_med->m_proto);
      strcat (buf, " \r\n");
    }
    pos++;
  }

  osip_message_set_body (answer, buf, strlen (buf));
  osip_message_set_content_type (answer, "application/sdp");
  sdp_message_free (remote_sdp);
  return 0;
}
