TARGETS = httpd

CC = gcc
CFLAGS = -g

all: $(TARGETS) Makefile
	
test: httpd
	wget -np -e robots=off --recursive --level=1 --domains=acme.com http://acme.com
	sudo ./httpd acme.com/ 80

clean:
	rm -f $(TARGETS)
	rm -f *.o *~

.PHONY: all clean
