MOTIF=-I/usr/X11R6/include -L/usr/X11R6/lib -lXm 
MMXMPCR_DEVICE = \"/dev/ttyUSB0\"

all: mmxmpcr
	@echo Done

mmxmpcr: mmxmpcr.cpp
	g++ -O0 -g3 -Wall $(MOTIF) -lpthread -DMMXMPCR_DEVICE=$(MMXMPCR_DEVICE) -o mmxmpcr mmxmpcr.cpp

clean:
	rm -f mmxmpcr core *.o

install: all
	cp mmxmpcr /usr/local/bin

uninstall:
	rm /usr/local/bin/mmxmpcr
