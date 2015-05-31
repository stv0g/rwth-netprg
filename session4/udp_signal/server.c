#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int fd;

void echo(int sig)
{		
	char line[1024];

	struct sockaddr_in sa = { .sin_family = AF_INET };	
	socklen_t salen = sizeof(sa);

	ssize_t msglen = recvfrom(fd, &line, sizeof(line), 0, (struct sockaddr *) &sa, &salen);
	if (msglen == -1)
		error(-1, errno, "Failed to recv");
		
	if (sendto(fd, &line, msglen, 0,
		(struct sockaddr *) &sa, salen) == -1)
		error(-1, errno, "Failed to send");
}

void quit()
{
	if (close(fd))
		error(-1, errno, "Failed to close socket");
}

int main(int argc, char *argv[])
{
	if (argc != 2)
		error(-1, 0, "Usage: %s PORT\n", argv[0]);
	
	struct sockaddr_in sa = { .sin_family = AF_INET };

	sa.sin_port = atoi(argv[1]);
	if (!sa.sin_port)
		fprintf(stderr, "Failed to parse port number");

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		error(-1, errno, "Failed to create socket");

	if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)))
		error(-1, errno, "Failed to bind socket");
	
	if (fcntl(fd, F_SETOWN, getpid()))
		error(-1, errno, "Failed fcntl");
	
	if (fcntl(fd, F_SETFL, O_ASYNC, 1))
		error(-1, errno, "Failed fcntl");
	
	atexit(quit);

	struct sigaction sig = {
		.sa_handler = echo,
		.sa_flags = 0
	};
	sigemptyset(&sig.sa_mask);

	if (sigaction(SIGIO, &sig, NULL))
		error(-1, errno, "sigaction");
	
	while(1);

	return 0;
}