TARGETS = httpd

CC = gcc
CFLAGS = -g

all: $(TARGETS) Makefile
	
test: httpd
	sudo ./httpd acme.com/ 80

clean:
	rm -f $(TARGETS)
	rm -f *.o *~

.PHONY: all clean
