#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

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
#include <sys/select.h>
#include <sys/time.h>

#include "readline.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

#define HTTP(c, m) (struct status) { .code = c, .msg = m }

static const char err[] = "\r\n<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
			  "\r\n<html><body><h1>%u %s</h1></body></html>\r\n";

int request(int outfd) {
	int closing = 1;
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
			/* Parse Headers */
			char header[1024];
			while (readline(outfd, header, sizeof(header))) {
				char *key = strtok(header, ":");
				char *value = strtok(NULL, "\r\n");
				
				if (strcmp(key, "\r\n") == 0)
					break;
				
				if (key && value) {
					printf("Header: %s => %s\n", key, value);

					if (strcasecmp(key, "Connection") == 0 && strcasecmp(value+1, "keep-alive") == 0)
						closing = 0;
				}
			}
			
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
		return 1;

	printf("   <<< HTTP/1.1 %u %s\r\n", rc.code, rc.msg);
	
	dprintf(outfd, "HTTP/1.1 %u %s\r\n", rc.code, rc.msg);
	//dprintf(outfd, "Connection: close\r\n");

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
	
	return closing;
}

int main(int argc, char *argv[]) {
	int val = 1, accept_fd;
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

	accept_fd = socket(sap->sa_family, SOCK_STREAM, 0);
	if (accept_fd < 0)
		error(-1, errno, "Failed to create socket");
	if (setsockopt(accept_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		error(-1, errno, "Failed setsockopt");
	if (bind(accept_fd, sap, salen))
		error(-1, errno, "Failed to bind socket");
	if (listen(accept_fd, 10))
		error(-1, errno, "Failed to listen on socket"); 
	if (chroot(argv[1]))
		error(-1, errno, "Failed to enter chroot");

#ifdef GAI
	freeaddrinfo(res);
#endif
	
	int pool[] = { [0 ... 31] = -1 };
	fd_set readfds;
	
	for (;;) {
		int result;
		/* Waiting for new data */
		do {
			FD_ZERO(&readfds);
			FD_SET(STDIN_FILENO, &readfds);
			FD_SET(accept_fd, &readfds);

			int nfds = accept_fd;
			for (int i = 0; i < ARRAY_LEN(pool); i++) {
				if (pool[i] >= 0) {
					FD_SET(pool[i], &readfds);
					nfds = pool[i] > nfds ? pool[i] : nfds;
				}
			}
			
			result = select(nfds + 1, &readfds, NULL, NULL, NULL);
		} while (result == -1 && errno == EINTR);
		
		/* New connection? */
		if (FD_ISSET(accept_fd, &readfds)) {
			/* Search emptly slot in pool */
			for (int i = 0; i < ARRAY_LEN(pool); i++) {
				if (pool[i] < 0) {
					struct sockaddr_storage peer;
					socklen_t peerlen = sizeof(peer);
					
					pool[i] = accept(accept_fd, (struct sockaddr *) &peer, &peerlen);
					if (pool[i] < 0)
						error(-1, errno, "Failed accept");
					
					char pname[NI_MAXHOST], pserv[NI_MAXSERV];
					int ret = getnameinfo((struct sockaddr *) &peer, peerlen,
					    pname, sizeof(pname), pserv, sizeof(pserv), 0);
					if (ret)
						printf("New connection: (%s) => %s:%s\n",
							gai_strerror(ret), lname, lserv);
					else
						printf("New connection: %s:%s => %s:%s\n",
							pname, pserv, lname, lserv);
					
					break;
				}	
			}
		}
		
		/* Handle existing connections */
		for (int i = 0; i < ARRAY_LEN(pool); i++) {
			if (FD_ISSET(pool[i], &readfds)) {
				if (request(pool[i])) {
					printf("Closing connection...\n");

					shutdown(pool[i], SHUT_RDWR);
					pool[i] = -1;
				}
			}
		}
		
		/* Key pressed? */
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			switch (getchar()) {
				case 'q': /* Quit server */
					goto quit;
				
				case 'k': /* Close all connections immeadiately */
					printf("Killing all connections!\n");

					for (int i = 0; i < ARRAY_LEN(pool); i++) {
						close(pool[i]);
						pool[i] = -1;
					}
					break;
				
				default:
					fprintf(stderr, "Unknown key\n");
			}
		}
	}
	
quit:
	printf("Goodbye!\n");

	return 0;
}
