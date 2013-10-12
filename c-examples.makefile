# make -f c-examples.makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -pedantic
all: capture-jpeg list-controls list-formats

capture-jpeg: capture.h capture.c c-examples/capture-jpeg.c
	$(CC) $(CFLAGS) capture.c c-examples/capture-jpeg.c -ljpeg -o $@

list-controls: capture.h capture.c c-examples/list-controls.c
	$(CC) $(CFLAGS) capture.c c-examples/list-controls.c -o $@

list-formats: capture.h capture.c c-examples/list-formats.c
	$(CC) $(CFLAGS) capture.c c-examples/list-formats.c -o $@

clean:
	rm -f capture-jpeg list-controls list-formats
