#include <stdio.h>

unsigned char get_value(unsigned short crc);

void init_tabl();

unsigned char count_crc8(unsigned char* pbuf, int len, unsigned char seed);
