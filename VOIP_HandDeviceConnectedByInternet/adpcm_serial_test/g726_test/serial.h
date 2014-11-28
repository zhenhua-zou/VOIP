#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>     /*标准函数库定义*/
#include <unistd.h>     /*Unix标准函数定义*/
#include <sys/types.h>  /**/
#include <sys/stat.h>   /**/
#include <fcntl.h>      /*文件控制定义*/
#include <errno.h>      /*错误号定义*/
#include <sys/times.h>
#include <unistd.h>
#include <linux/termios.h>

/* the following three function return >0 if OK, return -1 if error */


/* initial serial port 0, using the following default value:
 * speed = , 8 bit, parity , stop bits
 * return file descriptor
 */
int serial_init(void);	

/* waiting for the input binary voice data from serial port
 * return the length of the input data, return -1 if error*/
int serial_in_block(int fd, unsigned char s[], int length);

#endif
