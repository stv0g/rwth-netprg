TARGETS = echo_server echo_client

CC = gcc
CFLAGS = -g -D$(DEFINES)

all: $(TARGETS) Makefile
clean:
	rm -f $(TARGETS)
	rm -f *.o *.~

echo_server: echo_server.o
echo_client: echo_client.o	

.PHONY: all clean
