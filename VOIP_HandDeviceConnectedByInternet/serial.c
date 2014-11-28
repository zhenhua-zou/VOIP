#include "serial.h"

extern call_t calls;

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
  Opt.c_cc[VTIME] = 1;	/* set time out time, default to 0.1 seconds now, zouzhenhua need to be changed */
  Opt.c_cc[VMIN] = 0;
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

  return fd;
  
}


int serial_out_char(int fd, unsigned char c)
{
  unsigned char i[5];

  i[0] = 0x99; /* synchronization frame */
  i[1] = 0x01; /* length frame */
  i[2] = c;
  i[3] = count_crc8(i, 3, 0); /* crc-8  */
    
  if (4 != write(fd, i, 4)) {
    printf("write out control out error!\n");
    return -1;
  }

  return 0;
  
}


int serial_in_char(int fd, unsigned char* c)
{
  unsigned char i[5];

  /* syn frame */
  if (((read(fd, &i[0], 1)) != 1) || (i[0] != 0x99)) 
    return -1;

  /* length frame */
  if ((read(fd, &i[1], 1)) != 1)
    return -1;
  if (i[1] != 0x01) {
    printf("length error!\n");    
    return -1;
  }

  /* data output to c */
  if ((read(fd, &i[2], 1)) != 1)
    return -1;
  *c = i[2];
  
  /* crc correction */
//  if ( (read(fd, &i[3], 1) != 1) || (i[3] != count_crc8(i, 3, 0)) ) {
//      printf("crc error!\n");
//    return -1;
//  }
  
  return 0;
}


int serial_out_block(int fd, unsigned char s[], int length)
{
  unsigned char i[2];
  unsigned char j;
  
  i[0] = 0x9A; 			/* voice data frame */
  i[1] = length;		/* length  */

  j = count_crc8(i, 2, 0); 	/* crc - 8 */
  
  if (2 != write(fd, i, 2)) {
    printf("write error!\n");
    return -1;
  }
  
  if (length != write(fd, s, length)) {
    printf("write error!\n");
    return -1;
  }

  if (1 != write(fd, &j, 1)) {
    printf("write error!\n");
    return -1;
  }
  
  return 0;
  
}

/* temp implementation */
int serial_in_block(int fd, unsigned char s[], int length)
{
  unsigned char i[2];
  unsigned char j;
  int actual_length;
  
  if ( (2 != read (fd, i, 2)) || ((i[0] != 0x9A)&&(i[0] != 0x99)) || (i[1] != length)) {
//    printf("read error!\n");
    return -1;
  }

  if (i[0] == 0x99) {
    if ((1 == read (fd, i ,1) && (i[0] == 0x03))) {
      eXosip_lock ();
      j = eXosip_call_terminate (calls.cid, calls.did);
      if (j == 0)
	call_remove ();
      eXosip_unlock ();
      calls.enable_audio = -1;
    }
    return -1;
  }
  
  actual_length = read(fd, s, length);
  if (actual_length != length) {
    printf("read error!\n");
    return -1;
  }

  if ( (1 != read(fd, &j, 1)) || (j != count_crc8(i, 2, 0)) ) {
    printf("crc error!\n");
    return -1;
  }
  
  return actual_length;
  
}

/*int main()
{
  int fd;
  char c;
  char s[]="hello world\n\0";
    
  fd = serial_init();

  serial_out_char(fd, '0x31');
  
  serial_out_block(fd, s, sizeof(s));
  
  serial_in_char(fd, &c);
}*/

