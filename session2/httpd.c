#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include <netinet/ip.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "readline.h"

#define HTTP(c, m) (struct status) { .code = c, .msg = m }

static const char err[] = "\r\n<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
			  "\r\n<html><body><h1>%u %s</h1></body></html>\r\n";

static int fd;

void quit() {
	close(fd);
}

void request(int outfd) {
	struct stat st;
	struct status{ int code; const char *msg; } rc;

	int infd;
	char *method, *path, *version, line[1024];
	if (readline(outfd, line, sizeof(line)) > 0) {
		method = strtok(line, " ");
		path = strtok(NULL, " ");
		version = strtok(NULL, "\r\n");

		if (!method || !path || !version)
			rc = HTTP(400, "Bad Request");

		if (strcasecmp(method, "GET"))
			rc = HTTP(405, "Method Not Allowed");
		else if (strcasecmp(version, "HTTP/1.1"))
			rc = HTTP(505, "HTTP Version not supported");
		else {
			if (!strcmp(path, "/"))
				path = "/index.html";
		
			errno = 0;
			if (stat(path, &st) || !S_ISREG(st.st_mode))
				goto err;
	
			infd = open(path, O_RDONLY);
err:			switch (errno) {
				case 0:			rc = HTTP(200, "OK"); break;
				case EACCES:		rc = HTTP(403, "Forbidden"); break;
				case ENOENT:		rc = HTTP(404, "Not Found"); break;
				case ENAMETOOLONG:	rc = HTTP(414, "Request-URL Too Long"); break;
				default:		rc = HTTP(500, "Internal Server Error"); break;
			}
		}

		printf("   >>> %s %s %s\r\n", method, path, version); /* debug */
	}
	else
		rc = HTTP(400, "Bad Request");

	printf("   <<< HTTP/1.1 %u %s\r\n", rc.code, rc.msg);
	
	dprintf(outfd, "HTTP/1.1 %u %s\r\n", rc.code, rc.msg);
	dprintf(outfd, "Connection: close\r\n");

	switch (rc.code) {
		case 200:
			dprintf(outfd, "Content-length: %lu\r\n\r\n", st.st_size);

			if (!sendfile(outfd, infd, 0, st.st_size))
				error(-1, errno, "sendfile");
				
			close(infd);
			break;

		default:
			dprintf(outfd, err, rc.code, rc.msg);
			break;
	}
}

int main(int argc, char *argv[]) {
	int val = 1;
	char lname[NI_MAXHOST], lserv[NI_MAXSERV];

	if (argc != 3 && argc != 4)
		error(-1, 0, "usage: %s HTDOCS PORT\n", argv[0]);

	char *host = argc == 4 ? argv[3] : NULL;
	char *port = argv[2];

#ifdef GAI
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_PASSIVE | AI_ADDRCONFIG,
		.ai_socktype = SOCK_STREAM
	}, *res;

	if (getaddrinfo(host, port, &hints, &res))
		error(-1, errno, "Failed to get socket address");

	struct sockaddr *sap = res[0].ai_addr;
	socklen_t salen = res[0].ai_addrlen;
#else
    #ifdef IPV6
	struct sockaddr_in6 sa = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = htons(atoi(argv[2]))
	};
    #else
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(atoi(argv[2]))
	};
    #endif
	struct sockaddr *sap = (struct sockaddr *) &sa;
	socklen_t salen = sizeof(sa);
#endif

	if (getnameinfo(sap, salen,
	    lname, sizeof(lname), lserv, sizeof(lserv), 0))
		error(-1, errno, "Failed to get name");

	fd = socket(sap->sa_family, SOCK_STREAM, 0);
	if (fd < 0)
		error(-1, errno, "Failed to create socket");
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		error(-1, errno, "Failed setsockopt");
	if (bind(fd, sap, salen))
		error(-1, errno, "Failed to bind socket");
	if (listen(fd, 10))
		error(-1, errno, "Failed to listen on socket"); 
	if (chroot(argv[1]))
		error(-1, errno, "Failed to enter chroot");

	atexit(quit);
#ifdef GAI
	freeaddrinfo(res);
#endif

	for (;;) {
		struct sockaddr_storage peer;
		socklen_t peerlen = sizeof(peer);
	
		int fd2 = accept(fd, (struct sockaddr *) &peer, &peerlen);

		char pname[NI_MAXHOST], pserv[NI_MAXSERV];
		if (getnameinfo((struct sockaddr *) &peer, peerlen,
		    pname, sizeof(pname), pserv, sizeof(pserv), 0))
			error(-1, errno, "Failed to get name of peer");

		printf("%s:%s => %s:%s\n",
			pname, pserv, lname, lserv);

		if (fork() == 0) {
			close(fd);
			request(fd = fd2);
			shutdown(fd, SHUT_RDWR);
			exit(0);
		}
		else
			close(fd2);
	}

	return 0;
}
