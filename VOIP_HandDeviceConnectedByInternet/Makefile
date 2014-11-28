MODCFLAGS :=  -g #-O2
XSCALE_PATH := /opt/hardhat/devkit/arm/xscale_be/target/

COMPILE := xscale_be-gcc $(MODCFLAGS) -I$(XSCALE_PATH)/include
#SIPLINK := xscale_be-ld 

SRC := $(wildcard *.c)

VOIP_LIB := -lpthread -leXosip2 -losip2 -losipparser2 -lortp -lc -lgcc 

voip:$(SRC)
	@echo "compile begin..."
	@echo "file : $(SRC)"
	$(COMPILE) $(SRC) $(VOIP_LIB) -L$(XSCALE_PATH)/lib \
	-L$(XSCALE_PATH)/usr/lib/gcc-lib/armv5teb-harhat-linux/3.2.1 -o voip

clean:
	rm -f voip
