CCOPTS= -Wall -ggdb -std=gnu99 -Wstrict-prototypes
LIBS=
CC=gcc
AR=ar


BINS= simplefs_test

OBJS = simplefs_test.c simplefs.c bitmap.c disk_driver.c

HEADERS=simplefs.h\
        disk_driver.h\
        bitmap.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS)

simplefs_test: $(OBJS)
		$(CC) $(CCOPTS)  -o $@ $^ $(LIBS)

gdb:
	gdb ./simplefs_test

clean:
	rm -rf *.o *~  $(BINS)

test:
	./simplefs_test
