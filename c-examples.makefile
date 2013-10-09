# make -f c-examples.makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
all: capture-jpeg list-controls

capture-jpeg: capture.h capture.c c-examples/capture-jpeg.c
	$(CC) $(CFLAGS) capture.c c-examples/capture-jpeg.c -ljpeg -o $@

list-controls: capture.h capture.c c-examples/list-controls.c
	$(CC) $(CFLAGS) capture.c c-examples/list-controls.c -o $@

clean:
	rm -f capture-jpeg list-controls
