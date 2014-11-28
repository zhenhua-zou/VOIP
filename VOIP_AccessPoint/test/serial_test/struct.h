#ifndef _STRUCT_H
#define _STRUCT_H

/* message type the sip message block */
typedef enum SipMSG_Type
{
    INVITE = 0x00,
    REGISTER,
    TRANSFER,
    RING,
    OK,
    BYE,
    ERROR = 0xFFFFFFFF
}SipMSG_Type_t;

/* sip message block */
typedef struct SipMSG
{
    SipMSG_Type_t msg_type; 
    short sdr_id;
    short ap_id;
    int ap_rtp_port;
    int pc_rtp_port;
    int rssi;
    long time_stamp;
}
SipMSG_t;

/* serial message block */
typedef struct SerialMSG
{
    unsigned char begin_syn;            /* when transmit it should be configured to 0x55 */
    short sdr_id;
    short ap_id;
    unsigned char msg_type;
    unsigned char channel;
    unsigned char data_length;       /* the length of data */
    unsigned char* data;
    unsigned char rssi;
    unsigned char CRC;
    char end_syn;
}
SerialMSG_t;

typedef struct cfg
{
    char *ap_ip;
    short ap_id;
    char *pc_ip;
    struct sockaddr_in pc_addr;
    short sdr_id;
    short ap_rtp_port;
    short pc_rtp_port;
    int sip_socket;
    int rtp_socket;
    char* serial_port_name[2];
    int serial_control;
    int serial_voice;
    char* ring_name;
    int ring_lock;
    int rssi;
}
cfg_t;

#define SIP_SIZE sizeof(SipMSG_t)
#define SERIAL_SIZE sizeof(SerialMSG_t)
#define SERIAL_DATA_LENGTH 100
#define USED 1
#define FREE 0
#define PC_SIP_PORT 5060
#define AP_SIP_PORT 5060
#define VOICE_LENGTH 120

#endif
