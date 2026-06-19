
# CLAUDE.AI USAGE:
# PROMPT:
# [
# assume there exists a file, writer.c containing the program entrypoint.
# Write a Makefile which includes:
# A default target which builds the “writer” application
# A clean target which removes the “writer” application and all .o files
# ]
# RESPONSE USAGE: 
# copied base 'writer' and 'clean' implementation.
# PROMPT:
# [
# I want to make special behavior for the 'writer' target iff the make variable FOO is defined (on the command line), how do I do that?
# ]
# RESPONSE USAGE: learned about ifdef.

CC := gcc
CFLAGS := -Wall -Wextra -O2

ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)$(CC)
endif

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

writer: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f writer $(OBJ)

.PHONY: clean