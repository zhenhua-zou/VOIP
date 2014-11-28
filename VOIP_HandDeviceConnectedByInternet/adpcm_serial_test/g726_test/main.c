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
  int serial_fd;
  int sound_fd;
  int file_orig_fd;
  int file_encode_fd;
  int file_decode_fd;

  unsigned char in[120];
  unsigned char out[960];

  serial_fd = serial_init();
  sound_fd = sound_init();
  file_orig_fd = open("phone_orig.wav", O_RDONLY);
  if (file_orig_fd < 0) {
    printf("open phone.wav error! \n");
    return -1;
  }

  file_encode_fd = open("phone_encode.wav", O_RDWR|O_CREAT);
  if (file_encode_fd < 0) {
    printf("open file error !\n");
    return -1;
  }

  file_decode_fd = open("phone_decode.wav", O_WRONLY|O_CREAT);
  if (file_decode_fd < 0) {
    printf("open file error !\n");
    return -1;
  }
  
  /* the encoding part */
  int m;
  g726_state state_ptr;
  g726_init_state(&state_ptr);

  while (1) {
    m = read(file_orig_fd, out, 960);
    if (m != 960)
      break;
    g726_Encode(out, in, &state_ptr);
    write(file_encode_fd, in,120);
    }

  /* move the file offset to the begnning of the file */
  lseek(file_encode_fd, 0, SEEK_SET);

  g726_init_state(&state_ptr);
  /* decoding part */
  while(1) {
    m = read(file_encode_fd, in, 120);
    if (m != 120)
      break;
    g726_Decode(in, out, &state_ptr);
    write(file_decode_fd, out, 960);
  }

  close(serial_fd);
  close(sound_fd);
  close(file_orig_fd);
  close(file_encode_fd);
  close(file_decode_fd);
}
