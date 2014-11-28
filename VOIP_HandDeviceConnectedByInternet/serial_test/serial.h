#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>     /*标准函数库定义*/
#include <unistd.h>     /*Unix标准函数定义*/
#include <sys/types.h>  /**/
#include <sys/stat.h>   /**/
#include <fcntl.h>      /*文件控制定义*/
//#include <termios.h>    /*PPSIX终端控制定义*/
#include <errno.h>      /*错误号定义*/
#include <sys/times.h>
#include <unistd.h>
#include <linux/termios.h>
//#include "main.h"

/* the following three function return >0 if OK, return -1 if error */


/* initial serial port 0, using the following default value:
 * speed = , 8 bit, parity , stop bits
 * return file descriptor
 */
int serial_init(void);	


/* output one character through serial port
 * It may be the synchronization frame, length frame,
 * the control frame, and the CRC correction frame
 */
int serial_out_char(int fd, unsigned char c); 


/* waiting for one character input from serial port
 * It may be the synchronization frame, length frame,
 * the control frame, and the CRC correction frame
 * the return value is in the pointer c
 */
int serial_in_char(int fd, unsigned char* c); 


/* the data block's length is constant */

/* output a block of data through serial port
 * It should be the variable length of binary voice data
 * String s is the output data,
 * return -1 if error, return number less than length means
 * not all the data has been output, return 0 if OK
 */
int serial_out_block(int fd, unsigned char s[], int length);


/* waiting for the input binary voice data from serial port
 * return the length of the input data, return -1 if error*/
int serial_in_block(int fd, unsigned char s[], int length);

/* crc8 correction routine
 * pbuf is the input data, len means the length of the pbuf, seed should be set to 0
 */
unsigned char count_crc8( unsigned char* pbuf, int len, unsigned char seed );

#endif
