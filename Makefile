KERNEL_VERSION=`uname -r`
MODULE_DIR=/lib/modules/$(KERNEL_VERSION)/kernel/drivers/usb
SOURCE_INCLUDE_DIR=/usr/src/linux-$(KERNEL_VERSION)/include
MOTIF=-I/usr/X11R6/include -L/usr/X11R6/lib -lXm 
MODULE=mmxmpcr.o
MMXMPCR_MAJOR_NUMBER = 189
MMXMPCR_DEVICE = \"/dev/mmxmpcr\"

all: $(MODULE) mmxmpcr
	@echo Done

mmxmpcr: mmxmpcr.cpp
	g++ -O0 -g3 -Wall $(MOTIF) -DMMXMPCR_DEVICE=$(MMXMPCR_DEVICE) -o mmxmpcr mmxmpcr.cpp

mmxmpcr.o: mmxmpcr.c
	gcc -D__KERNEL__ -I$(SOURCE_INCLUDE_DIR) -DMODULE -c -DMMXMPCR_MAJOR_NUMBER=$(MMXMPCR_MAJOR_NUMBER) -O -Wall mmxmpcr.c

clean:
	rm -f mmxmpcr core *.o

install: all
	cp $(MODULE) $(MODULE_DIR)
	/sbin/depmod
	cp mmxmpcr /usr/local/bin
	-mknod /dev/mmxmpcr c $(MMXMPCR_MAJOR_NUMBER) 0

uninstall:
	rm $(MODULE_DIR)/$(MODULE)
	-/sbin/rmmod mmxmpcr
	rm /usr/local/bin/mmxmpcr

load: $(MODULE)
	/sbin/modprobe mmxmpcr

unload:
	/sbin/rmmod mmxmpcr



