TARGETS = server client echo_server echo_client

CC = gcc
CFLAGS = -g

ifdef DEFINES
	CFLAGS += -D$(DEFINES)
endif

all: $(TARGETS) Makefile
clean:
	rm -f $(TARGETS)
	rm -f *.o *~

server: server.o
client: client.o

# Compile with echo support
echo_server: server.c
	$(CC) $(CFLAGS) -DECHO $^ -o $@

echo_client: client.c
	$(CC) $(CFLAGS) -DECHO $^ -o $@

.PHONY: all clean
