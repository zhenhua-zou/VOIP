#ifndef _FUNC_H
#define _FUNC_H_

#include "main.h"

/**
 * initialize sip socket
 * 
 * @return return 0 if OK, return -1 if ERROR
 */
int serial_init();

/**
 * initialize audio serial port and control serial port
 * 
 * @return return 0 if OK, return -1 if ERROR
 */
int sip_init();

/**
 * receive specified sip message type from sip socket in the specified timeout
 * and transmit it to the control serial port
 * the error is not transmitted in this routine and if no message is specified, the transmitted is not carried out in this routine
 * 
 * @param msg_type the specified message type from the sip socket
 *                 if it equals 0xffffffff, it will receive any type of message
 * @param timeout  timeout value in ms
 * 
 * @return return the received message type
 *         return -1 if it receives ERR message or timeout
 */
SipMSG_Type_t sip_serial_txd(SipMSG_Type_t msg_type, int timeout);


/**
 * receive specified serial message type from control serial port  in the specified timeout
 * and transmit it to the sip socket
 * the error is not transmitted in this routine and if no message is specified, the transmitted is not carried out in this routine
 * 
 * @param msg_type the specified message type from the control serial port if it equals 0xff, it will receive any type of message
 * @param timeout  timeout value in ms
 * 
 * @return return the received message type
 *         return -1 if it receives ERR message or timeout
 */
unsigned char serial_sip_txd(unsigned char msg_type, int timeout);


/* copy sip message format to serial message format */
void sip_serial_cpy();


/* copy serial message format to sip message format */
void serial_sip_cpy();


/**
 * transmit transfer message toate audio serial port
 * 
 * @return return -1 if ERROR, 0 if OK
 */
int transfer_notify();


/**
 * transmit serial message to specified serial port
 * 
 * @param fd     the file descriptor of the serial port
 * @param msg    the serial message
 * 
 * @return return -1 if ERROR, 0 if OK
 */
int serial_txd(int fd, SerialMSG_t msg);


/**
 * receive serial message from specified serial port
 * 
 * @param fd     the file descriptor of the serial port
 * @param msg    memory area to store the serial message
 * 
 * @return return -1 if ERROR, 0 if OK, -2 if nothing is read
 */
int serial_rxd(int fd, SerialMSG_t* msg);


/* transmit RING audio data to audio serial port
 */
void* ring_txd(void* arg);


/**
 * compare the rssi value to determine whether it is necessary to transfer the ap
 * 
 * @param rssi   the received rssi value this time
 * 
 * @return return 1 if it needs trasnfer, 0 if it doesn't need
 */
int rssi_compare(unsigned char rssi);

/**
 * initialize rtp transmision, return 0 if OK, -1 if ERROR
 * 
 * @param pc_addr the destination pc addr
 * 
 * @return 0 if OK, -1 if ERROR
 */
int rtp_init(struct sockaddr_in* pc_addr);

/* close rtp transmission */
int rtp_close();

#endif
