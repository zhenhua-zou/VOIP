MODCFLAGS := -O2
XSCALE_PATH := /opt/hardhat/devkit/arm/xscale_be/target/

COMPILE := xscale_be-gcc $(MODCFLAGS) -I$(XSCALE_PATH)/include

#VOIP_LIB := -lpthread -leXosip2 -losip2 -losipparser2 -lortp -lc -lgcc 

bld : serial.c crc-8.c
	$(COMPILE) serial.c crc-8.c $(VOIP_LIB) -L$(XSCALE_PATH)/lib  -o bld

