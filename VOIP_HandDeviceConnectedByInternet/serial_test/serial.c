#include "serial.h"
#include "crc-8.h"

int serial_init(void)
{
  int fd;
  struct termios Opt;
  int speed = B115200;
  int status;
  
  /* open device */
  fd = open("/dev/ttyS5", O_RDWR | O_NOCTTY | O_NDELAY);
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

  tcflush(fd, TCIOFLUSH);  

  return fd;
  
}


int serial_out_char(int fd, unsigned char c)
{
  unsigned char i[5];

  i[0] = 0x99; /* synchronization frame */
  i[1] = 0x01; /* length frame */
  i[2] = c;
  i[3] = count_crc8(i, 3, 0); /* crc-8  */
    
  if (write(fd, i, 4) != 4) {
    printf("write control out error!\n");
    return -1;
  }

  return 0;
  
}

/* catch input of control frame */
int serial_in_char(int fd, unsigned char* c)        
{
  unsigned char *i;
  i = (unsigned char*)malloc(5);

  /* set to nonblocking mode */
  fcntl(fd, F_SETFL, FNDELAY);
  
  /* catch input buffer */
  if ((read(fd, i, 1)) == -1) {
    free(i);
    return -1;
  }

  /* check whether the input is syn frame */
  if (*i != 0x99) {
    free(i);
    return -2;
  }

  /* set to blocking mode */
  fcntl(fd, F_SETFL, 0);

  /* catch one input of length*/
  if ((read(fd, i+1, 1)) != 1) {
    free(i);
    return -3;
  }
  /* check for length */
  if (*(i+1) != 0x01) {
    free(i);
    return -4;
  }

  /* catch one input of data */
  if ((read(fd, i+2, 1)) != 1) {
    free(i);
    return -5;
  }

  *c = *(i+2);

  /* crc correction */
  if ( (read(fd, i+3, 1) != 1) || (*(i+3) != count_crc8(i, 3, 0)) ) {
    free(i);
    return -6;
  }
  
  free(i);

  /* return 0 means everything is OK */
  return 0;
}


int serial_out_block(int fd, unsigned char s[], int length)
{
  unsigned char i[2];
  unsigned char j;
  
  i[0] = 0x9A; 			/* voice data frame */
  i[1] = length;		/* length  */

  j = count_crc8(i, 2, 0); 	/* crc - 8 */
  
  if (write(fd, i, 2) != 2 ) {
    printf("write data out error!\n");
    return -1;
  }
  
  if (write(fd, s, length) != length) {
    printf("write data out error!\n");
    return -2;
  }

  if (1 != write(fd, &j, 1)) {
    printf("write data out error!\n");
    return -3;
  }
  
  return 0;
  
}

/* catch input of voice data block */
/* temp implementation */
/* parameter length is the anticipated, return value is the actual */ 
int serial_in_block(int fd, unsigned char *s, int length)
{
  unsigned char* i;
  i = (unsigned char*)malloc(5);		/* i is for the control frame */

  /* set to nonblocking mode */
  fcntl(fd, F_SETFL, FNDELAY);
  
  /* catch input buffer */
  if ((read(fd, i, 1)) == -1) {
    free(i);
    return -1;
  }

  /* set to blocking mode */
  fcntl(fd, F_SETFL, 0);

  /* check whether the input is syn frame */
  if (*i == 0x99) {
    /* input is the control frame, looking for the terminating signal */

//    printf("control frame!\n"); /* zouzhenhua */
    
    /* the length frame */
    if ((read(fd, i+1, 1) != 1) || *(i+1) != 0x01) {
      free(i);
      return -3;
    }

    /* whether the control frame is the ternimating signal */
    if ((read(fd, i+2, 1) != 1) || *(i+2) != 0x03) {
      free(i);
      return -4;
    }
    
    /* calcuate the crc correction */
/*    if ((read(fd, i+3, 1) != 1) || *(i+3) != count_crc8(i, 3, 0)) {
      free(i);
      return -5;
      }*/

    /* terminate the call */
    if (0) {
      int m;
/*      eXosip_lock ();
	m = eXosip_call_terminate (calls.cid, calls.did);
	if (m == 0)
	call_remove ();
	eXosip_unlock ();
	calls.enable_audio = -1;*/
    }
 
    free(i);
    return 0;
  } else if (*i == 0x9A) {
    /* inupt is the data frame, process the following data */
    
    /* the length frame */
    int len;
    if(read(fd, i+1, 1) == -1) {
      free(i);
      return -5;
    }
    len = (int)*(i+1);
    if ((len <= 0) || (len > length)) { /* unsupported length */
      free(i);
      return -6;
    }

    /* implementaion 1 */
    int actual, m;
    m = 0;
    actual = 0;
    while ((actual < len)&&(m < 10)) {
      ioctl(fd, FIONREAD, &actual);
      usleep(100); 		
      m++;
    }

    if (actual >= len) {
      m = read(fd, s, len);
    } else {
      printf("acutal is %d \n",actual);
      printf("len is %d \n",len);      
      tcflush(fd, TCIOFLUSH);
      return -7;
    }
    
    /* implementation 2 : modify the VMIN value*/
        
    free(i);
    return m;
  } else {
    free(i);
    return -2;
  }
  
  return 0;
  
}

int main()
{
  int fd;
  unsigned char *i;
  int m;

  fd = serial_init();
  if (fd == -1)
    printf("error!\n");

  i = (unsigned char*)malloc(50);

  /* crc-test */
/*  {
    unsigned char test_string[100]="hello world!\n";
    unsigned char check;
    check = count_crc8(test_string, strlen(test_string),0);
    printf("\ncheck is 0x%x\n",check);
  }
  return 0;*/
  
  while (1) {
/*    m = serial_in_char(fd, i);
      if (m < 0) {
      printf("error code is %d !\n",m);
      }
      usleep(1000000);
      continue;*/

    m = serial_in_block(fd, i, 20);
    if (m < 0) {
      printf("error code is %d \n",m);
      usleep(1000000);
      continue;
    } else {
      printf("main input length is %d \n",m);
    }
    *(i+m) = '\0';    
    printf("input string is: %s \n",i);
    
    usleep(1000000);
  }

  free(i);
  
//  serial_out_block(fd, s, sizeof(s));
  
//  serial_in_char(fd, &c);
}

