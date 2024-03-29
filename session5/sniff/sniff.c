#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include <arpa/inet.h>
#include <net/if.h>

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define SNAPLEN		1024

const char* icmp_type_str[] = {
	[ 0 ] = "Echo Reply",
	[ 1 ... 2 ] = NULL,
	[ 3 ] = "Destination Unreachable",
	[ 4 ] = "Source Quench",
	[ 5 ] = "Redirect",
	[ 6 ] = "Alternate Host Address",
	[ 7 ] =  NULL,
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

int hexdump(char *buf, size_t len)
{
	uint16_t *data = (uint16_t *) buf;
	size_t i;
	for (i = 0; i < len; i += 2) {
		/* ASCII */
		if (i % 16 == 0 && i != 0) {
			printf("\t");
			for (size_t j = i-16; j < i; j++)
				printf("%c", isprint(buf[j]) ? buf[j] : '.');
			printf("\n");
		}
		/* Address offset */
		if (i % 16 == 0)
			printf("\t\t%#06x:\t", i);
		
		printf("%04hx ", ntohs(data[i/2]));
	}

	printf("\t");
	for (; i < len; i++)
		printf("%c", isprint(buf[i]) ? buf[i] : '.');
	printf("\n");
}

int parse_packet(char *data, size_t len)
{
	int protocol;
	char *next_hdr;
	static int cnt;

	printf("%5d >>> Received packet with %u bytes:\n", cnt++, len);

layer2:	{
	struct ethhdr *eth = (struct ethhdr*) data;
	printf("\tEthernet src: %s dst: %s type: %#04x\n",
		ether_ntoa((const struct ether_addr*) eth->h_source),
		ether_ntoa((const struct ether_addr*) eth->h_dest),
		ntohs(eth->h_proto)
	);

	protocol = ntohs(eth->h_proto);
	next_hdr = (char *) (eth + 1);
}


layer3:	switch (protocol) {
		case ETH_P_IP: {
			/* Parse IP protocol */
			struct iphdr *ip = (struct iphdr*) next_hdr;
			char buf[32];
		
			printf("\tIP version: %u ihl: %u ttl: %u protocol: %u src: %s dst %s\n",
				ip->version,
				ip->ihl,
				ip->ttl,
				ip->protocol,
				inet_ntop(AF_INET, &ip->saddr, buf, sizeof(buf)),
				inet_ntop(AF_INET, &ip->daddr, buf, sizeof(buf))
			);

			protocol = ip->protocol;
			next_hdr = (char*) ip + ip->ihl * 4;
			goto layer4;
		}
	
		case ETH_P_IPV6: {
			struct ip6_hdr *ip6 = (struct ip6_hdr*) next_hdr;
			char buf[32];

			printf("\tIP version: %u class: %u flow: %u hlim: %u: next: %u src: %s dst %s\n",
				ip6->ip6_vfc >> 4,
				ip6->ip6_vfc & 0xf,
				ip6->ip6_flow,
				ip6->ip6_hlim,
				ip6->ip6_nxt,
				inet_ntop(AF_INET6, &ip6->ip6_src, buf, sizeof(buf)),
				inet_ntop(AF_INET6, &ip6->ip6_dst, buf, sizeof(buf))
			);

			protocol = ip6->ip6_nxt;
			next_hdr = (char*) (ip6 + 1);
			goto layer4;
		}
		
		case ETH_P_ARP:
			printf("\tARP\n");
			goto dump;

		default:
			goto end;
	}
	
layer4:	switch (protocol) {
		case IPPROTO_ICMP: {
			/* Parse ICMP */
			struct icmphdr *icmp = (struct icmphdr*) next_hdr;

			printf("\tICMP type: %u (%s) code: %u\n",
				icmp->type,
				icmp_type_str[icmp->type],
				icmp->code
			);

			goto dump;
		}

		case IPPROTO_UDP: {
			/* Parse UDP */
			struct udphdr *udp = (struct udphdr*) next_hdr;

			printf("\tUDP src: %u dst: %u len: %u\n",
				ntohs(udp->source),
				ntohs(udp->dest),
				ntohl(udp->len)
			);
			goto layer5;
		}

		case IPPROTO_TCP: {
			/* Parse TCP */
			struct tcphdr *tcp = (struct tcphdr*) next_hdr;

			printf("\tTCP src: %u dst: %u seq: %u win: %u ",
				ntohs(tcp->source),
				ntohs(tcp->dest),
				ntohl(tcp->seq),
				ntohs(tcp->window)
			);

			if (tcp->fin)	printf("FIN ");
			if (tcp->syn)	printf("SYN ");
			if (tcp->rst)	printf("RST ");
			if (tcp->ack)	printf("ACK ");

			printf("\n");

			if (tcp->fin || tcp->syn || tcp->rst)
				goto end;
			else
				goto layer5;
		}

		default:
			goto end;
	}

dump:
layer5:
	hexdump(next_hdr, len - (next_hdr - data));

end:
	printf("\n");

	return 0;
}

int main (int argc, char *argv[])
{
	uint8_t packet[SNAPLEN];
	int val, sd, ifindex;
	struct ifreq ifr;
	struct sockaddr_ll sll;

	if (argc != 2)
		error(-1, 0, "Usage %s INTERFACE", argv[0]);

	sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd < 0)
		error(-1, errno, "Failed to create socket");

	val = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
		error(-1, errno, "Failed to set socket option: SO_REUSEADDR");

	/* Get interface flags */
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
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
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_ALL);
	sll.sll_ifindex = ifr.ifr_ifindex;

	if (bind(sd, (struct sockaddr*) &sll, sizeof(sll)))
		error(-1, errno, "Failed to bind socket to interface");

	printf("Successfully put interface %s (%u) into promiscuous mode!\n",
		ifr.ifr_name, ifr.ifr_ifindex);
	printf("The socket is now bound/listening on interface %s\n", ifr.ifr_name);

	while (1) {
		ssize_t bytes = recv(sd, packet, sizeof(packet), 0);
		if (bytes == -1)
			error(-1, errno, "Failed to recv");

		parse_packet(packet, bytes);
	}
	
	close(sd);
	
	return 0;
}
