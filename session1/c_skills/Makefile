TARGETS = ass1 ass2

CC = gcc
CFLAGS = -g

ifdef DEFINES
	CFLAGS += -D$(DEFINES)
endif

all: $(TARGETS) Makefile
clean:
	rm -f $(TARGETS)
	rm -f *.o *.~

ass1: ass1.o
ass2: ass2.o

.PHONY: all clean
