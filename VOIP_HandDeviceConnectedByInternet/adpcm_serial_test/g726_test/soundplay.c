#include <stdio.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

int sound_init()
{
  int p, cond;
  int stereo = 1;               /* 1 is mono */
  int rate = 8000;		/* sampling rate */
  int blocksize = 512;
  int frag;
  int FMT = AFMT_S16_LE;
  int fd;
  int i;
  int min_size;
  
  fd = open ("/dev/dsp", O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    {printf("block error! \n");return -EWOULDBLOCK;}
  fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);

  ioctl(fd, SNDCTL_DSP_RESET, 0);

/* the fragment size is 2^9 = 512, 7fff is the max number of the frag */
  frag = ((0x7fff<<16) | 9); 	/* 0x7fff0009 */
  if(ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag) == -1) {
    printf("open,: can't set fragment size: %s", strerror(errno));
  }
      
  p = FMT;  	/* using standard 16-bit signed little-endian sample format */
  if((ioctl(fd, SNDCTL_DSP_SETFMT, &p)) == -1) {
    printf("open, can't set sample format: %s", strerror(errno));
  }
  if (p != FMT) {
    printf("can't set FMT!\n");
  }
      
  p = stereo;			/* mono */
  i = ioctl(fd, SNDCTL_DSP_CHANNELS, &p);
  if(i == -1) {
    printf("open, can't set strereo type: %s", strerror(errno));
  }
  if( p != stereo) {
    printf("warning, can't set stereo to %d, but to %d instead !\n", stereo, p);
  }

  p = rate;			/* sample rate is 8k */
  i = ioctl(fd, SNDCTL_DSP_SPEED, &p);
  if(i == -1) {
    printf("open, can't set sampling speed: %s", strerror(errno));
  }
  if(p != rate) {
    printf("can't set sampling speed!\n");
  }

  /* check whether the block size is settled successfully */
  ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &min_size);
  if(min_size>blocksize) {
    printf("minsize large than blocksize\n min_size: %d\n blocksize: %d\n" \
	   ,min_size, blocksize);
    return -1;
  }
  else {
    min_size = blocksize;
  }

  return fd;
}

