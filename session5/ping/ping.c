#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define __USE_BSD 1
#include <netinet/in_systm.h>
#include <netinet/ip_icmp.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define ICMP_FILTER 1
#define ECHO_DATA "Hello World"

int hexdump(void *buf, size_t len)
{
	uint16_t *data = (uint16_t *) buf;
	
	for (size_t i = 0; i < len; i += 2) {
		if (i % 16 == 0 && i != 0)
			printf("\n");
		
		printf("%04hx ", ntohs(data[i/2]));
	}
	printf("\n");
}

uint16_t icmp_checksum(void *vdata, size_t length)
{
	// Cast the data pointer to one that can be indexed.
	char *data = (char*)vdata;

	// Initialise the accumulator.
	uint32_t acc = 0xffff;

	// Handle complete 16-bit blocks.
	for (size_t i = 0; i + 1 < length; i += 2) {
		uint16_t word;
		memcpy(&word, data+i,2);
		acc += ntohs(word);
		if (acc > 0xffff)
			acc -= 0xffff;
	}

	// Handle any partial block at the end of the data.
	if (length & 1) {
		uint16_t word = 0;
		memcpy(&word, data+length-1, 1);
		acc += ntohs(word);
		if (acc > 0xffff)
			acc -= 0xffff;
	}

	// Return the checksum in network byte order.
	return htons(~acc);
}

int ping(int sd, uint16_t seq, uint16_t id)
{
	struct icmp pkg = {
		.icmp_type = ICMP_ECHO,
		.icmp_code = 0,
		.icmp_cksum = 0,
		.icmp_id = htons(id),
		.icmp_seq = htons(seq)
	};

	strcpy(pkg.icmp_data, ECHO_DATA);
		
	size_t pkg_len = 8 + strlen(pkg.icmp_data);
	pkg.icmp_cksum = icmp_checksum(&pkg, pkg_len);
	
	ssize_t len = send(sd, &pkg, pkg_len, 0);
	
	return len < 0 ? -1 : 0;
}

int pong(int sd, uint16_t seq, uint16_t id)
{
	ssize_t len;
	char buf[128];
		
	len = recv(sd, &buf, sizeof(buf), 0);
	if (len < 0)
		return -1;
	
	uint8_t ihl = buf[0] & 0xf;
	uint8_t ttl = buf[8];
	
	struct icmp *pkg = (struct icmp *) &buf[ihl * 4];
	
	if (!icmp_checksum(&pkg, len))
		return -2;
	
	if (pkg->icmp_type != ICMP_ECHOREPLY ||
	    pkg->icmp_code != 0)
		return -3;
	
	if (pkg->icmp_id != htons(id) ||
	    pkg->icmp_seq != htons(seq))
		return -4;
	
	if (strcmp(ECHO_DATA, pkg->icmp_data))
		return -5;
	
	printf("len: %u, ttl: %u, type: %u, code: %u, id: %u, seq: %u, data: %10s", len-ihl*4, ttl,
		pkg->icmp_type, pkg->icmp_code, ntohs(pkg->icmp_id), ntohs(pkg->icmp_seq), pkg->icmp_data);
	
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	struct timespec ts_start, ts_end;
	struct sockaddr_in sa = {
		.sin_family = AF_INET
	};
	
	if (argc != 2)
		error(-1, 0, "Usage: %s IP-ADDRESS", argv[0]);
	
	if (inet_pton(AF_INET, argv[1] , &sa.sin_addr) != 1)
		error(-1, 0, "Failed to parse IP address");
	
	int sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sd < 0)
		error(-1, errno, "Failed to create socket");
	
	if (connect(sd, (struct sockaddr*) &sa, sizeof(sa)))
		error(-1, errno, "Failed to connect socket");
	
	/* We only want to receive echo replies */
	uint32_t data = ~(1 << ICMP_ECHOREPLY);	
	if (setsockopt(sd, SOL_RAW, ICMP_FILTER, &data, sizeof(data)))
		error(-1, errno, "Failed socketopt");
	
	time_t t;
	time(&t);
	srand((unsigned int) t);
	
	uint16_t seq = 0;
	uint16_t id = rand();
	for (; seq < 10; seq++) {
		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		
		ret = ping(sd, seq, id);
		if (ret)
			error(-1, errno, "Failed ping: %d (%s)", ret, strerror(errno));
		
		do {
			ret = pong(sd, seq, id);
			if (ret)
				error(0, errno, "Failed pong: %d (%s)", ret, strerror(errno));
		} while(ret);
		
		clock_gettime(CLOCK_MONOTONIC, &ts_end);
		
		double rtt = 1e3 * (ts_end.tv_sec - ts_start.tv_sec) + 1e-6 * (ts_end.tv_nsec - ts_start.tv_nsec);
		printf(", rtt: %f ms\n", rtt);
		
		sleep(1);
	}
	
	close(sd);
	
	return 0;
}