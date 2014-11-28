#include "serial.h"

int serial_init(void)
{
  int fd;
  struct termios Opt;
  int speed = B115200;
  int status;
  
  /* open device */
  fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    printf("open device error");
    return -1;
  }

  /* set to normal(blocking) behavior
   * the timeout length is set in the parity mode set in the follow
   */
  fcntl(fd, F_SETFL, 0);
  
  /* set input and output serial speed = 115200 */
  tcgetattr(fd, &Opt);
  tcflush(fd, TCIOFLUSH);
  cfsetispeed(&Opt, speed);
  cfsetospeed(&Opt, speed);
  status = tcsetattr(fd, TCSANOW, &Opt);
  if (status != 0) {
    printf("set speed error");
    return -1;
  }
  tcflush(fd, TCIOFLUSH);

  /* set parity mode to 8N1 */
  tcgetattr(fd, &Opt);
  Opt.c_cflag &= ~PARENB;
  Opt.c_cflag &= ~CSTOPB;
  Opt.c_cflag &= ~CSIZE;
  Opt.c_cflag |= CS8;
  Opt.c_iflag &= ~INPCK; 	/* don't check parity */
  Opt.c_cc[VTIME] = 20;	/* set time out time, default to 0.1 seconds now, zouzhenhua need to be changed */
  Opt.c_cc[VMIN] = 10;
  tcflush(fd, TCIFLUSH);
  if (tcsetattr(fd, TCSANOW, &Opt) != 0) {
    printf("set parity mode error!\n");
    return -1;
  }

  /* set raw input and output  */
  tcgetattr(fd, &Opt);
  Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  Opt.c_iflag |= IGNCR;
  Opt.c_oflag &= ~OPOST;
  tcsetattr(fd, TCSANOW, &Opt);

  tcflush(fd, TCIOFLUSH);  

  return fd;
  
}

/* catch input of voice data block */
/* temp implementation */
/* parameter length is the anticipated, return value is the actual */ 
int serial_in_block(int fd, unsigned char *s, int length)
{
  unsigned char* i;
  i = (unsigned char*)malloc(5);		/* i is for the control frame */
}


