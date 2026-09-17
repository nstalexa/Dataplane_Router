#                                README



## `trie.c` file

### `create_node` function:

This is a simple function that completes the fields of the structure `TrieNode` in order to represent a node in our trie. Each node has an `end_of_word` mark, a route table entry and the children of that node.
The `children` field is implemented as an array of size 2, corresponding to the binary nature of IP addresses (each bit being either 0 or 1). This design allows efficient traversal based on individual bits of the destination IP address.


### `insert_node` function:

This function receives the root for the trie and a route table entry.  We want to store the relevant bits only, so we save the mask length and add in the children. We mark the end of word true and add the rtable. 
The function computes the mask length by counting the number of consecutive bits set to 1. After determining the prefix length, the function iterates through the corresponding bits of the IP prefix, starting from the most significant bit. 
For each bit, it decides whether to traverse left (0) or right (1) in the trie.

### `longest_prefix_match` function:

The algorithm works by traversing the trie bit by bit. While traversing, it keeps track of the last node that contained a valid route (`rtable != NULL`). This assures the return of the closest match to the given address that could be found. 

The trie has been implemented specifically to make the match search a lot easier in constant time rather than polynomial.

---


## `router.c` file

###  `get_arp_entry` function:

This function's purpose is to return an arp table entry of the given IP (`destination`), in order to easily fetch the correspunding arp table to any IP if needed.

### `send_req_arp` function:

This function builds a request ARP and sends it. First, it builds the ether header, setting the destination to the broadcast address, ensuring that all devices on the local network will receive the request.  
After that, it builds an ARP header and completes its fields accordingly. Lastly, the ether header and ARP header are copied in a packet and sent. The main loop then continuously listens for incoming packets on any interface, making the 
router capable of handling traffic.

### `main`:

A trie is built using the routing table entries. It enables the efficient LPM algorithm, storing only relevant bits of each route. Then, an ARP table is allocated, which will dynamically store mappings between IP addresses and MAC addresses as ARP
 replies are received. A global queue is created to temporarily store packets that cannot be forwarded now (waiting packets) due to not enough ARP information. 
Then, the router enters the main loop that receives packets and each packet is treated differently depending on its ethernet type. Afterwards, the ethernet header is extracted and we enter two cases: IPv4 (0x0800) and ARP (0x0806). 

---

### IP case


The checksum is recalculated and compared to the old sum. If they differ, we drop the packet. 

If the packet is destined to the router, we have to check if the received packet is a request (mtype = 8).  If the type is a request, it transforms the packet into reply, swaps the source and dest IPs, updates checksums and sends the packet back. 

If the packet is not destined to the router then we get the longest prefix match and, if it is not found, an ICMP destination unreachable message (mtype 3) is generated and sent. By convention, we set the TTL at 64. 
If the IP header TTL is less or equal to 1, an ICMP time limit exceeded (mtype 11) is generated and sent.

This mechanism prevents packets from circulating indefinitely in the network due to routing loops. Each router decrements the TTL, and when it reaches zero, the packet is discarded and an ICMP error is sent back. 
This is also the principle behind tools like traceroute.


The TTL is decreased, checksum recalculated and an ARP table entry is created. If the entry for the next hop is null, a request ARP is send in order to wait for more information about the ARP corresponding to the IP address and the the packet
 is enqueued to the global queue. Else, the packet is sent. The processing continues with the next packet.


---

### ARP case

When an ARP request is received, the packet is modified into an ARP reply, the MAC and IP addresses are swapped and the reply is sent back.
The router responds only if the request targets one of its interfaces. By swapping sender and target fields and updating the opcode to reply, the router provides its MAC address to the requester. 

 In case of ARP reply, the ARP table is updated and the packets waiting for this MAC are sent, while the others are inserted back into the queue.

The router now has the necessary mapping between IP and MAC address. This mapping is stored in the ARP table for future use. The queue is then processed: packets waiting for this specific mapping are sent immediately, while others remain queued. 

---

