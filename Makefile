CC=gcc
DEPS = fastboot.h usb.h
OBJ_OSX = protocol.o engine.o fastboot.o usb_osx.o util_osx.o 
SRC_OSX = protocol.c engine.c fastboot.c usb_osx.c util_osx.c  
OBJ_LINUX = protocol.o engine.o fastboot.o usb_linux.o util_linux.o 
SRC_LINUX = protocol.c engine.c fastboot.c usb_linux.c util_linux.c  

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

linux: $(OBJ_LINUX)
	$(CC) -Wall -lpthread $(SRC_LINUX) -o fastboot

osx: $(OBJ_OSX)
	$(CC) -Wall -lpthread -framework CoreFoundation -framework IOKit -framework Carbon $(SRC_OSX) -o fastboot

clean:
	rm -f *.o
	rm -f fastboot
