CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS=
CC=gcc
AR=ar


BINS= simplefs_test

OBJS= 	disk_driver.o\
	#bitmap.o

HEADERS=simplefs.h\
        disk_driver.h\
        bitmap.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS)

simplefs_test:	simplefs_test.c $(OBJS)
		$(CC) $(CCOPTS)  -o $@ $^ $(LIBS)

clean:
	rm -rf disk *.o *~  $(BINS)
