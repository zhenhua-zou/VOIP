#include <stdio.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>

int
main(void)
{
  int p, cond;
  int stereo = 1;               /* 1 is mono */
  int rate = 8000;
  int blocksize = 512;
  int frag;
  int fd;
  int min_size;
  int i;
  int sound;
  char data[100000];
  struct timeval first, second;
  int FMT = AFMT_S16_LE;

  fd = open ("/dev/dsp", O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    return -EWOULDBLOCK;
  fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);

  ioctl(fd, SNDCTL_DSP_RESET, 0);

  frag = ((0x7fff<<16) | 9);
  if(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag) == -1) {
    printf("open,: can't set fragment size: %s", strerror(errno));
  }
      
  p = FMT;
  if((ioctl(fd, SNDCTL_DSP_SETFMT, &p)) == -1) {
    printf("open, can't set sample format: %s", strerror(errno));
  }
  if (p != FMT) {
    printf("can't set FMT");
  }
      
  p = stereo;
  i = ioctl(fd, SNDCTL_DSP_CHANNELS, &p);
  if(i == -1) {
    printf("open, can't set strereo type: %s", strerror(errno));
  }
  if( p != stereo) {
    printf("warning, can't set stereo to %d, but to %d instead !\n", stereo, p);
  }

  p = rate;
  i = ioctl(fd, SNDCTL_DSP_SPEED, &p);
  if(i == -1) {
    printf("open, can't set sampling speed: %s", strerror(errno));
  }
  if(p != rate) {
    printf("can't set sampling speed!\n");
  }

  ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &min_size);
  if(min_size > blocksize) {
    cond = 1;
    p = min_size/blocksize;
    while(cond) {
      i = ioctl(fd, SNDCTL_DSP_SUBDIVIDE, &p);
      if ((i==0)||(p==1)) cond = 0;
      else p=p/2;
    }
  }
  
  ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &min_size);
  if(min_size>blocksize) {
    printf("minsize large than blocksize error\n"); return -1;}
  else {
    min_size = blocksize;
  }
  
  sound = open("record.wav", O_WRONLY|O_CREAT);
  if (sound < 0) {
    printf("open record.wav error! \n");
    return -1;
  }

  gettimeofday(&first, 0);

  i = 0 ;

  while(read(fd, data, 512)>0) {
    write(sound, data, 512);
    i++;
    gettimeofday(&second, 0);
    if((second.tv_sec - first.tv_sec)>4) break;
  }
  
  printf("%d\n",i);
  close(fd);
}
  
  
