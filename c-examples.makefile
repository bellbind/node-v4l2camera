# make -f c-examples.makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wunused-parameter -pedantic
LDLIBS = -ljpeg

capturesrc := capture.h capture.c
srcdir := c-examples
mains := $(wildcard $(srcdir)/*.c)
targets := $(patsubst $(srcdir)/%.c,%,$(mains))

all: $(targets)

$(targets): %: $(srcdir)/%.c $(capturesrc)
	$(CC) $(CFLAGS) $(filter %.c, $^) $(LDLIBS) -o $@

clean:
	rm -f $(targets)
