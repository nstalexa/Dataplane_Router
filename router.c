#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "trie.h"


struct arp_table_entry *get_arp_entry(uint32_t destination, struct arp_table_entry *arp_table, int arp_table_len) {

	for(int i = 0; i < arp_table_len; i++) {
		if(arp_table[i].ip == destination) {
			return &arp_table[i];
		}
	}

	//Here I just searched through the ip's and returned 
	//the arp entry for the target/destination. 

	return NULL;
}

void send_req_arp(uint32_t next, int interface) {

	//Function to send a request arp.
	struct ether_hdr * ether = malloc(sizeof(struct ether_hdr));

	ether->ethr_type = htons(0x0806); //type IPv4
	uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	//Default broadcast address.
	memcpy(ether->ethr_dhost, broadcast, 6);

	get_interface_mac(interface, ether->ethr_shost);

	struct arp_hdr * arp = malloc(sizeof(struct arp_hdr));
	arp->hw_type = htons(1);
	arp->proto_type = htons(0x0800);
	arp->hw_len = 6;
	arp->proto_len = 4;
	arp->opcode = htons(1);
	//Update the arp to be a request type arp.

	get_interface_mac(interface, arp->shwa);
	inet_pton(AF_INET, get_interface_ip(interface), &arp->sprotoa);
	arp->tprotoa = next;

	int len = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
	char packet[len];
	memcpy(packet, ether, sizeof(struct ether_hdr));
	memcpy(packet + sizeof(struct ether_hdr), arp, sizeof(struct arp_hdr));

	send_to_link(len, packet, interface);
	//Send the request arp.
}

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);


	struct route_table_entry *rtable = malloc(sizeof(struct route_table_entry) * 70000);
	int rtable_len = read_rtable(argv[1], rtable);

	TrieNode * root = create_node();
	for(int i = 0; i < rtable_len; i++) {
		insert_node(root, ntohl(rtable[i].prefix), &rtable[i]);
	}
	//This is a Trie created by adding each node to the root.
	//This Trie will help us search more efficiently for the
	//Longest Prefix Match algorithm.

	struct arp_table_entry *arp_table = malloc(sizeof(struct arp_table_entry) * 70000);
	int arp_table_len = 0;

	queue q = create_queue();
	//Queue for waiting packets.

	while (1) {

		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

    // TODO: Implement the router forwarding logic
		struct ether_hdr *eth_header = (struct ether_hdr *) buf;

		//Got the ether header for the packet (buf).

		if(ntohs(eth_header->ethr_type) == 0x0800) {
			//If it's an IPv4 header we enter a separate case.

			struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));
			uint16_t old = ip_header->checksum;
			ip_header->checksum = 0;

			uint16_t new = htons(checksum((uint16_t *)(ip_header), sizeof(struct ip_hdr)));

			//We recalculate the checksum in order to see if it
			//changed (therefore the content has been corrupted) or not.

			if(old != new) {
				//If corrupted, drop.
				continue;
			}

			ip_header->checksum = new;

			uint32_t router_ip;
			inet_pton(AF_INET, get_interface_ip(interface), &router_ip);

			if(ip_header->dest_addr == router_ip) {
				//If the destination address is our router's address, 
				//we check if it's a request and change it to reply, 
				//switching it's destination and sender addresses.
				struct icmp_hdr * icmp = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));

				if(icmp->mtype == 8) {

					uint32_t aux = ip_header->dest_addr;
					ip_header->dest_addr = ip_header->source_addr;
					ip_header->source_addr = aux;

					ip_header->proto = 1;
					ip_header->checksum = 0;
					ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

					icmp->mtype = 0;
					icmp->mcode = 0;

					icmp->check = 0;
					icmp->check = htons(checksum((uint16_t *)icmp, len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr)));

					memcpy(eth_header->ethr_dhost, eth_header->ethr_shost, 6);
					get_interface_mac(interface, eth_header->ethr_shost);
					send_to_link(len, buf, interface);

					//We send the new, transformed, reply back to where it came from.
					
				}
				continue;
				
			}

			struct route_table_entry * entry = longest_prefix_match(root, ntohl(ip_header->dest_addr));

			//Searching for the next hop the packet should make in order to 
			//reach the destination. If it's NULL, it means something happened
			//and it's route is wrong, or the destination is unreachable.

			if(entry == NULL) {
				//If the next hop is unreachable, we need to send
				//an ICMP packet with an error message.
				char err[sizeof(struct ip_hdr) + 8];
				memcpy(err, ip_header, sizeof(struct ip_hdr) + 8);
				//It should contain the ip header and 64 bits from
				//the original payload.
				ip_header->ttl = 64;
				ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(err));

				ip_header->dest_addr = ip_header->source_addr;
				inet_pton(AF_INET, get_interface_ip(interface), &ip_header->source_addr);

				ip_header->proto = 1;
				ip_header->checksum = 0;
				ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

				//Update the ip header in order to send it back to source addr.

				struct icmp_hdr * icmp = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
				icmp->mtype = 3;
				icmp->mcode = 0;
				memcpy((char *)icmp + sizeof(struct icmp_hdr), err, sizeof(err));

				icmp->check = 0;
				icmp->check = htons(checksum((uint16_t *)icmp, sizeof(struct icmp_hdr) + sizeof(err)));

				//Create the icmp header to send back, with the mtype 3 for 
				//unreachable destination.

				memcpy(eth_header->ethr_dhost, eth_header->ethr_shost, 6);
				get_interface_mac(interface, eth_header->ethr_shost);
				send_to_link(sizeof(struct ether_hdr) + ntohs(ip_header->tot_len), buf, interface);

				continue;

				//Send the buffer back to source.
			}


			if(ip_header->ttl <= 1) {

				char err[sizeof(struct ip_hdr) + 8];
				memcpy(err, ip_header, sizeof(struct ip_hdr) + 8);
				ip_header->ttl = 64;
				ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(err));

				ip_header->dest_addr = ip_header->source_addr;
				inet_pton(AF_INET, get_interface_ip(interface), &ip_header->source_addr);

				ip_header->proto = 1;
				ip_header->checksum = 0;
				ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

				struct icmp_hdr * icmp = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
				icmp->mtype = 11;
				icmp->mcode = 0;
				memcpy((char *)icmp + sizeof(struct icmp_hdr), err, sizeof(err));

				icmp->check = 0;
				icmp->check = htons(checksum((uint16_t *)icmp, sizeof(struct icmp_hdr) + sizeof(err)));

				memcpy(eth_header->ethr_dhost, eth_header->ethr_shost, 6);
				get_interface_mac(interface, eth_header->ethr_shost);
				send_to_link(sizeof(struct ether_hdr) + ntohs(ip_header->tot_len), buf, interface);

				continue;
				
				
			}
			//Same code as the one for the unreachable destination, this time
			//changing the mtype to 11 for time limit reached.


			ip_header->ttl--;
			ip_header->checksum = 0;

			ip_header->checksum = htons(checksum((uint16_t *)(ip_header), sizeof(struct ip_hdr)));

			struct arp_table_entry * arp_entr = get_arp_entry(entry->next_hop, arp_table, arp_table_len);
			if(arp_entr == NULL) {
				struct packet * p = malloc(sizeof(struct packet));
				memcpy(p->payload, buf, len);
				p->len = len;
				p->interface = entry->interface;
				p->next_hop = entry->next_hop;

				send_req_arp(entry->next_hop, entry->interface);
				queue_enq(q, (void *)p);
				continue;
			
			}

			memcpy(eth_header->ethr_dhost, arp_entr->mac, 6);
			get_interface_mac(entry->interface, eth_header->ethr_shost);

			send_to_link(len, buf, entry->interface);
		} else if (ntohs(eth_header->ethr_type) == 0x0806) {
			//In case of ARP
			//2 cases - send/req 
			//opcode == 1 -> request, opcode == 2 -> reply
			struct arp_hdr * arp = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

			if(arp->opcode == htons(1)) {
				//Case of request.
				int ip;
				inet_pton(AF_INET, get_interface_ip(interface), &ip);
				uint8_t mac[6];
				get_interface_mac(interface, mac);
				//Get the mac address.

				memcpy(eth_header->ethr_dhost, arp->shwa, 6);
				memcpy(eth_header->ethr_shost, mac, 6);
				//Update the destination and source with
				//the new mac addresses to send back.
				
				if(arp->tprotoa != ip) {
					continue; // The packet is not for us => drop.
				}

				arp->tprotoa = arp->sprotoa;
				arp->sprotoa = ip;
				//Update the arp ips, the arp macs and the opcode to reply.
				arp->opcode = htons(2);
				memcpy(arp->thwa, arp->shwa, 6);
				memcpy(arp->shwa, mac, 6);

				send_to_link(len, buf, interface);
				//Forward reply.
				
			} else if(arp->opcode == htons(2)) {
				//Case reply.
				struct arp_table_entry * arptable = malloc(sizeof(struct arp_table_entry));
				memcpy(arptable->mac, arp->shwa, 6);
				arptable->ip = arp->sprotoa;

				memcpy(&arp_table[arp_table_len], arptable, sizeof(struct arp_table_entry));
				arp_table_len++;

				queue new_q = create_queue();

				//Add reply to arp table.
				struct packet * p = malloc(sizeof(struct packet));
				while(!queue_empty(q)) {
					p = queue_deq(q);
					if(p->next_hop == arptable->ip) {
						struct ether_hdr *pether = (struct ether_hdr *)p->payload;
						memcpy(pether->ethr_dhost, arp->shwa, 6);
						get_interface_mac(p->interface, pether->ethr_shost);

						send_to_link(p->len, p->payload, p->interface);
					} else {
						queue_enq(new_q, p);
					}
				}

				//Go through queue, take all packets out, if the next hop is
				//the ip of the reply, we send the packets and don't add them
				//back. The others will be added back.

				q = new_q;

			} else {
				continue;
			}

		}
	}
}