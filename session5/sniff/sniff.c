#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#include <sys/socket.h>

#include <linux/if_packet.h>
#include <net/ethernet.h>

#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define SNAPLEN 1024

const char* icmp_type_str = {
	[ 0 ] = "Echo Reply",
	[ 1..2 ] = NULL,	
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
}

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
	struct ether_header *ehdr = data;
	
	printf("ether_src: %s ", ether_ntoa(ehdr->ether_shost));
	printf("ether_dst: %s ", ether_ntoa(ehdr->ether_dhost));
	printf("ether_type: %#04x ", ehdr->ether_type);
	
	if (ehdr->ether_type == ETHERTYPE_IP) {
		/* Parse IP protocol */
		struct ip *iphdr = (char *) ehdr + sizeof(*ehdr)
		
		printf("ip_version: %u ", iphdr->ip_v);
		printf("ip_ihl: %u ", iphdr->ip_hl);
		printf("ip_ttl: %u ", iphdr->ip_ttl);
		printf("ip_protocol: %u ", iphdr->ip_p);
		printf("ip_src: %u ", iphdr->ip_p);
		printf("ip_dst: %u ", iphdr->ip_p);
	
		if (iphdr->p == IPPROTO_ICMP) {
			/* Parse ICMP */
			struct icmphdr *icmphdr = (char *) iphdr + iphdr->ip_hl * 4;
			
			printf("icmp_type: %u (%s) ");
			printf("icmp_code: %u ")
		}
		else if (iphdr->p == IPPROTO_UDP) {
			/* Parse UDP */
			struct udphdr *udphdr = (char *) udphdr + iphdr->ip_hl * 4;
	
		}
		else if (iphdr->p == IPPROTO_TCP) {
			/* Parse TCP */
			struct udphdr *udphdr = (char *) udphdr + iphdr->ip_hl * 4;
			
		}
	}
}

int main (int argc, char *argv[])
{
	uint8_t packet[SNAPLEN];

	int sd = socket(AF_INET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd < 0)
		error(-1, errno, "Failed to create socket");
	
	
	while (1) {
		if (recv(sd, packet, sizeof(packet), 0) > 0)
			parse_packet(packet, sizeof(packet));
	}
	
	close(sd);
	
	return 0;
}