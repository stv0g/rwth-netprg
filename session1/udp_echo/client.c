#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void handle_error(char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));

	exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
	int fd;
	char *lineptr = NULL;
	size_t linelen = 0;

	struct sockaddr_in sa = { .sin_family = AF_INET };

	if (argc != 3) {
		printf("usage: %s DEST-IP PORT\n", argv[0]);
		exit(-1);
	}

	if (!inet_aton(argv[1], &sa.sin_addr))
		fprintf(stderr, "Failed to parse destination IP address\n");

	sa.sin_port = atoi(argv[2]);
	if (!sa.sin_port)
		fprintf(stderr, "Failed to parse port number");

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		handle_error("Failed to create socket");

	if (connect(fd, (struct sockaddr *) &sa, sizeof(sa)))
		handle_error("Failed to connect socket");

	while (!feof(stdin)) {
		size_t len = getline(&lineptr, &linelen, stdin);
		if (len > 0) {
			if (send(fd, lineptr, len+1, 0) == -1)
				handle_error("Failed to send");

#ifdef ECHO
			if (recv(fd, lineptr, linelen, 0) == -1)
				handle_error("Failed to recv");

			fprintf(stdout, "%s", lineptr);
#endif
		}
	}

	if (close(fd))
		handle_error("Failed to close socket");

	free(lineptr);

	return 0;
}
