TARGETS = server client echo_server echo_client

CC = gcc
CFLAGS = -g

all: $(TARGETS) Makefile
clean:
	rm -f $(TARGETS)
	rm -f *.o *~

# Compile with echo support
echo_server: server.c
	$(CC) $(CFLAGS) -DECHO $^ -o $@

echo_client: client.c
	$(CC) $(CFLAGS) -DECHO $^ -o $@

.PHONY: all clean
