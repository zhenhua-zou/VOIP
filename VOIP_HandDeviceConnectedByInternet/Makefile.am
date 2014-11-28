bin_PROGRAMS = voip

CFLAGS = @VOIP_FLAGS@ -g #-O

voip_SOURCES = \
commands.c commands.h calls.c calls.h mainconfig.h \
main.c main.h sdptools.c sdptools.h sound-ortp.c \
serial.h serial.c crc-8.c

voip_LDFLAGS = @VOIP_LIB@ 

voip_LDADD = -lrt

INCLUDES = -I./ -I$(top_srcdir) -I$(top_srcdir)/include -I$(includedir) $(MORE_INCLUDES)
