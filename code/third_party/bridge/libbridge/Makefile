CC := /opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-linux-gcc -c -g $(COPTS) -DNEED_PRINTF
LD := /opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-linux-ld

all:			libbridge.a

clean:
			rm -f *~
			rm -f *.o
			rm -f libbridge.a

libbridge.a:		libbridge_devif.o libbridge_if.o libbridge_init.o libbridge_misc.o
			$(LD) -r -o libbridge.a libbridge_devif.o libbridge_if.o libbridge_init.o libbridge_misc.o


libbridge_devif.o:	libbridge_devif.c libbridge.h libbridge_private.h
			$(CC) -o libbridge_devif.o libbridge_devif.c

libbridge_if.o:		libbridge_if.c libbridge.h libbridge_private.h
			$(CC) -o libbridge_if.o libbridge_if.c

libbridge_init.o:	libbridge_init.c libbridge.h libbridge_private.h
			$(CC) -o libbridge_init.o libbridge_init.c

libbridge_misc.o:	libbridge_misc.c libbridge.h libbridge_private.h
			$(CC) -o libbridge_misc.o libbridge_misc.c
