#include <stdio.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include "g72x.h"

int main()
{
  struct timeval time_before;
  struct timeval time_after;
  int serial_fd;
  int sound_fd;
  int file_fd;
  int file_decode_fd;
  int m = 0;
  int i = 0;
  unsigned char *in;
  unsigned char *out;

  serial_fd = serial_init();
  sound_fd = sound_init();
  file_fd = open("input.wav", O_RDWR|O_CREAT);
  file_decode_fd = open("decode.wav", O_WRONLY|O_CREAT);
  
  /* decoding part */
  in = (unsigned char*)malloc(150);
  out = (unsigned char*)malloc(1000);
  
  g726_state state_ptr;
  g726_init_state(&state_ptr);

/*  while(1) {
    m = read(file_fd, in, 120);
    if (m != 120)
      break;
    g726_Decode(in, out, &state_ptr);
    write(sound_fd, out, 960);
    i++;
  }

  printf("loop times is : %d!\n",i);
  printf("m is %d!\n",m);*/

  gettimeofday(&time_before, NULL);
  while(1) {
    m = read(serial_fd, in, 10);
    printf("i is %d\n",i);
    if (m != 10)
      break;
     g726_Decode(in, out, &state_ptr);
    write(file_fd, in, 10);
    i++;
    write(sound_fd, out, 80);
    if (i > 4800)
      break;
  }
  gettimeofday(&time_after, NULL);

  calculate_time(time_before, time_after);

  printf("i is %d\n", i);
  printf("m is %d\n", m);  
  close(serial_fd);
  close(sound_fd);
  close(file_fd);
  close(file_decode_fd);
  
  free(in);
  free(out);
}
