TARGETS = httpd httpd6 httpdgai

CC = gcc
CFLAGS = -g

all: $(TARGETS) Makefile

httpd6: CFLAGS=-DIPV6
httpdgai: CFLAGS=-DGAI

httpdgai httpd httpd6: httpd.c readline.c
	$(CC) $(CFLAGS) $^ -o $@

test: httpd
	sudo -i && ./httpd www 80 &
	wget http://localhost:80
	killall httpd

clean:
	rm -f $(TARGETS)
	rm -f *.o *~

.PHONY: all clean
