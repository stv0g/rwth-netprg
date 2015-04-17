#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#define HTTP_200 (struct status) { 200, "OK" }
#define HTTP_400 (struct status) { 400, "Bad request" }
#define HTTP_401 (struct status) { 401, "Unauthorized" }
#define HTTP_404 (struct status) { 404, "Not Found" }
#define HTTP_405 (struct status) { 405, "Method Not Allowed" }
#define HTTP_414 (struct status) { 414, "Request-URL Too Long" }
#define HTTP_413 (struct status) { 413, "Request Entity Too Large" }
#define HTTP_500 (struct status) { 500, "Internal Server Error" }

struct status {
	int code;
	const char *msg;
};

int fd;

const char *error_page = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
			 "<html><head><title>%1$u %2$s</title></head><body><h1>%2$s</h1></body></html>";

void handle_error(char *msg)
{
	perror(msg); exit(EXIT_FAILURE);
}

struct status errno_to_status()
{
	switch (errno) {
		case 0:            return HTTP_200;
		case EACCES:       return HTTP_401;
		case ENOENT:       return HTTP_404;
		case ENAMETOOLONG: return HTTP_414;
		case EOVERFLOW:    return HTTP_413;
		default:           return HTTP_500;
	}
}

void handle_connection(int outfd)
{
	char *method, *path, *version, *msg;
	int infd;
	struct stat st;
	struct status status;
	
	FILE *f = fdopen(outfd, "r+");
	if (!f)
		handle_error("Failed to open file descriptor");
	
	if (fscanf(f, "%ms %ms HTTP/%ms", &method, &path, &version) == 3) {		
		if (!strcmp(path, "/")) {
			if (!realloc(path, 32))
				handle_error("Failed realloc");
			strncpy(path, "/index.html", 32);
		}
		
		printf(">>> %s %s HTTP/%s\n", method, path, version);
		
		if (strcasecmp(method, "GET"))
			status = HTTP_405;
		else if (stat(path, &st))
			status = errno_to_status();
		else if ((infd = open(path, O_RDONLY)))
			status = errno_to_status();
	}
	else
		status = HTTP_400;
	
	printf("<<< HTTP/1.1 %u %s\r\n", status.code, status.msg);
	
	fprintf(f, "HTTP/1.1 %u %s\r\n", status.code, status.msg);
	fprintf(f, "Connection: close\r\n");
		
	switch (status.code) {
		case 200:
			fprintf(f, "Content-length: %u\r\n\r\n", st.st_size);
			sendfile(outfd, infd, 0, st.st_size);
			break;

		default:
			fprintf(f, error_page, status.code, status.msg);
			break;
	}
	
	
	shutdown(outfd, SHUT_RDWR);
}

void quit()
{
	close(fd);
}

int main(int argc, char *argv[])
{
	char htdocs[PATH_MAX] = "/var/www";
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(80)
	};
	
	if (argc > 3) {
		fprintf(stderr, "usage: %s HTDOCS_DIR [PORT]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (argc > 2)
		sin.sin_port = htons(atoi(argv[2]));
	if (argc > 1)
		strncpy(htdocs, argv[1], sizeof(htdocs));
	
	if (chroot(htdocs))
		handle_error("Failed to change root");
	
	atexit(quit); /* Close sockets */

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		handle_error("Failed to create socket");
	if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)))
		handle_error("Failed to bind socket");
	if (listen(fd, 10))
		handle_error("Failed to listen on socket");
	
	/* Main loop */
	for (;;) {
		int fd2 = accept(fd, NULL, 0);
		if (fork() == 0) {
			close(fd);
			handle_connection(fd = fd2);
			exit(0);
		}
		else
			close(fd2);
	}

	if (close(fd))
		handle_error("Failed to close socket");
	
	return 0;
}