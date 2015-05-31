#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include <net/if.h>

#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define SNAPLEN		65536
#define INTERFACE	"eth1"

const char* icmp_type_str[] = {
	[ 0 ] = "Echo Reply",
	[ 1 ... 2 ] = NULL,	
	[ 3 ] = "Destination Unreachable",
	[ 4 ] = "Source Quench",
	[ 5 ] = "Redirect",
	[ 6 ] = "Alternate Host Address",
	[ 7 ] = NULL,
	[ 8 ] = "Echo",
	[ 9 ] = "Router Advertisement",
	[ 10 ] = "Router Solicitation",
	[ 11 ] = "Time Exceeded",
	[ 12 ] = "Parameter Problem",
	[ 13 ] = "Timestamp",
	[ 14 ] = "Timestamp Reply",
	[ 15 ] = "Information Request",
	[ 16 ] = "Information Reply",
	[ 17 ] = "Address Mask Request",
	[ 18 ] = "Address Mask Reply"
};

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

int parse_packet(uint8_t *data, size_t len)
{
	/* Parse Ethertype */
	struct ethhdr *eth = (struct ethhdr*) data;
	
	printf("ether_src: %s ", ether_ntoa((const struct ether_addr*) eth->h_source));
	printf("ether_dst: %s ", ether_ntoa((const struct ether_addr*) eth->h_dest));
	printf("ether_type: %#04x ", eth->h_proto);
	
	if (eth->h_proto == ETH_P_IP) {
		/* Parse IP protocol */
		struct iphdr *ip = (struct iphdr*) (eth+1);
		
		printf("ip_version: %u ", ip->version);
		printf("ip_ihl: %u ", ip->ihl);
		printf("ip_ttl: %u ", ip->ttl);
		printf("ip_protocol: %u ", ip->protocol);
		printf("ip_src: %u ", *((struct in_addr*) &ip->saddr));
		printf("ip_dst: %u ", *((struct in_addr*) &ip->daddr));
	
		if (ip->protocol == IPPROTO_ICMP) {
			/* Parse ICMP */
			struct icmphdr *icmp = (struct icmphdr*) ((char*) ip + ip->ihl * 4);
			
			printf("icmp_type: %u (%s) ");
			printf("icmp_code: %u ");
		}
		else if (ip->protocol == IPPROTO_UDP) {
			/* Parse UDP */
			struct udphdr *udp = (struct udphdr*) ((char*) ip + ip->ihl * 4);
			
			printf("udp_port: %u ");
		}
		else if (ip->protocol == IPPROTO_TCP) {
			/* Parse TCP */
			struct tcphdr *tcp = (struct tcphdr*) ((char*) ip + ip->ihl * 4);
			
			printf("tcp_port: %u ");
		}
	}

	printf("\n");
}

int main (int argc, char *argv[])
{
	uint8_t packet[SNAPLEN];
	int val, sd, ifindex;
	struct ifreq ifr;
	struct sockaddr_ll sll;

	sd = socket(AF_INET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd < 0)
		error(-1, errno, "Failed to create socket");

	val = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		error(-1, errno, "Failed to set socket option: SO_REUSEADDR");

	/* Get interface flags */
	strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ);
	if (ioctl(sd, SIOCGIFFLAGS, &ifr))
		error(-1, errno, "Failed to get interface flags");
	
	/* Enable promiscious flag for interface */
	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl(sd, SIOCSIFFLAGS, &ifr))
		error(-1, errno, "Failed to enable promiscuous mode");

	/* Get interface index */
	if (ioctl(sd, SIOCGIFINDEX, &ifr))
		error(-1, errno, "Failed to get interface index");

	/* Bind socket to interface */
	memset(&sll, 0, sizeof(sll));
	sll.sll_protocol = htons(ETH_P_ALL);
	sll.sll_ifindex = ifr.ifr_ifindex;

	if (bind(sd, (struct sockaddr*) &sll, sizeof(sll)))
		error(-1, errno, "Failed to bind socket to interface");

	printf("Successfully put interface %s (%u) into promiscuous mode!\n",
		ifr.ifr_name, ifr.ifr_ifindex);
	printf("The socket is now bound/listening on interface %s", ifr.ifr_name);

	while (1) {
		ssize_t bytes = recv(sd, packet, sizeof(packet), 0);
		if (bytes == -1)
			error(-1, errno, "Failed to recv");

		printf("Received %u bytes\n", bytes);
		parse_packet(packet, sizeof(packet));
	}
	
	close(sd);
	
	return 0;
}
