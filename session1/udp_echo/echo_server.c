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
	char line[1024];
	ssize_t msglen;
	socklen_t salen;

	struct sockaddr_in sa = { .sin_family = AF_INET };

	if (argc != 2) {
		printf("usage: %s PORT\n", argv[0]);
		exit(-1);
	}

	sa.sin_port = atoi(argv[1]);
	if (!sa.sin_port)
		fprintf(stderr, "Failed to parse port number");


	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		handle_error("Failed to create socket");

	if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)))
		handle_error("Failed to bind socket");

	while (1) {
		salen = sizeof(sa);

		msglen = recvfrom(fd, &line, sizeof(line), 0, (struct sockaddr *) &sa, &salen)
		if (msglen == -1)
			handle_error("Failed to recv");

#ifdef ECHO
		if (sendto(fd, &line, msglen, 0,
			(struct sockaddr *) &sa, salen) == -1)
			handle_error("Failed to send");
#else
		fprintf(stdout, "%s", line);
#endif
	}

	if (close(fd))
		handle_error("Failed to close socket");

	return 0;
}
