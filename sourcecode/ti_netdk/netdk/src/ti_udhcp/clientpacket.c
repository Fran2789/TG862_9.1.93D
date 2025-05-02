/* clientpacket.c
 *
 * Packet generation and dispatching functions for the DHCP client.
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
//	1. The server_config structure is extended to an array to define dhcp server 
// configuration on a per interface basis. NSP supports multiple lan groups 
// and requires dhcp server configuration per lan groups. These configurations 
// are saved in the server_config array. udhcp server supports configuration for
//  upto 6 interfaces.
//  2. Modified the main() function accordingly to listen on upto 6 sockets. 
// lease_file is therefore defined on a per interface basis. auto_time variable 
// (timeout_end) is extended to an array to hold 6 entries. 
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license.
//-------------------------------------------------------------------------------------*/
 
#include <string.h>
#include <sys/socket.h>
#include <features.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "options.h"
#include "dhcpc.h"
#include "udhcp_debug.h"

int kernel_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
           u_int32_t dest_ip, int dest_port, char *interface);

extern void init_header(struct dhcpMessage *packet, char type);
extern u_int16_t checksum(void *addr, int count);
extern int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
	   u_int32_t dest_ip, int dest_port, unsigned char *dest_arp, int ifindex); 

/* Create a random xid */
unsigned long random_xid(void)
{
	static int initialized;
	if (!initialized) {
		srand(time(0));
		initialized++;
	}
	return rand();
}


static int udhcpc_plugin_option_hook(unsigned char *optionptr, char type)
{
	int len = 0;
	int end = end_option(optionptr);

	// ARRIS ADD : Check for buffer overrun
	if (end < 0) {
		LOG(LOG_ERR, "udhcpc_poh: Buffer overrun looking for end option");
		return -1;
	}
	// END ARRIS

	len = udhcpc_mcb.plugin.add_options(udhcp_get_client_state(),type, &optionptr[end]);

	if (len < 0)
		return len;
	optionptr[end + len] = DHCP_END;
	return 0;
}

/* initialize a packet with the proper defaults */
static int init_packet(struct dhcpMessage *packet, char type)
{
    int ret, len;
	unsigned char *temp;

	init_header(packet, type);
	memcpy(packet->chaddr, udhcp_client_config.arp, 6);
	if (udhcp_client_config.hostname) add_option_string(packet->options, udhcp_client_config.hostname);
	ret = udhcpc_plugin_option_hook(packet->options, type);
   
    /* Check if a valid CLIENT_ID option has been added by the plugin */
    if (((temp = get_option(packet, DHCP_CLIENT_ID,&len)) == NULL) || (len < 2)) {
        /* If a client id option not already present, add a default one */
        if(!temp)
	        add_option_string(packet->options, udhcp_client_config.clientid);
        /* If the client id is present, but it is incomplete/invalid print an error message. */
        else if(temp && len < 2)
	        LOG(LOG_ERR, "init_packet: invalid CLIENT_ID option in the packet, override it with defaults");
    }
    return ret;
}

/* Add a paramater request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static int add_requests(char msg_type, struct dhcpMessage *packet)
{
	int end = end_option(packet->options);
	int len;

	// ARRIS ADD : Handle buffer check failure
	if (end < 0) {
		LOG(LOG_ERR, "add_requests: Buffer overrun looking for end option");
		return -1;
	}
	// END ARRIS

	if ((len = udhcpc_mcb.plugin.add_param_request_list(udhcp_get_client_state(),msg_type, &packet->options[end])) < 0)
		return -1;	
	packet->options[end + len] = DHCP_END;

	return 0;
}


/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int send_discover(unsigned long xid, unsigned long requested)
{
	struct dhcpMessage packet;
/* ARRIS MODIFY*/
        int ret = 0;
/* END ARRIS MODIFY*/

	if (init_packet(&packet, DHCPDISCOVER) < 0) 
		return -1;

	packet.xid = xid;
	if (requested)
		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);

	
	if (add_requests(DHCPDISCOVER, &packet) <0)
		return -1;

	LOG(LOG_DEBUG, "Sending discover...");

/* ARRIS MODIFY*/
	ret = raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST, SERVER_PORT, MAC_BCAST_ADDR, udhcp_client_config.ifindex);
       
    udhcpc_mcb.plugin.convert_packet_to_log(&packet);
    return ret;
/* END ARRIS MODIFY*/

}


/* Broadcasts a DHCP request message */
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested)
{
	struct dhcpMessage packet;
	struct in_addr addr;
/* ARRIS MODIFY*/
    int ret = 0;
/* END ARRIS MODIFY*/

	if (init_packet(&packet,DHCPREQUEST ) < 0) 
		return -1;

	packet.xid = xid;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	if (add_requests(DHCPREQUEST, &packet) <0)
		return -1;

	addr.s_addr = requested;
	LOG(LOG_DEBUG,"Sending select for %s...", inet_ntoa(addr));
/* ARRIS MODIFY*/
	ret = raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST, SERVER_PORT, MAC_BCAST_ADDR, udhcp_client_config.ifindex);

    udhcpc_mcb.plugin.convert_packet_to_log(&packet);
    return ret;
/* END ARRIS MODIFY*/

}


/* Unicasts or broadcasts a DHCP renew message */
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr)
{
	struct dhcpMessage packet;
	int ret = 0;

	if (init_packet(&packet,DHCPREQUEST ) < 0) 
		return -1;
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	if (add_requests(DHCPREQUEST, &packet) <0)
		return -1;

	LOG(LOG_DEBUG, "Sending renew...");
	if (server) 
		ret = kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT,udhcp_client_config.interface);
	else ret = raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, udhcp_client_config.ifindex);

/* ARRIS MODIFY*/
    udhcpc_mcb.plugin.convert_packet_to_log(&packet);
/* END ARRIS MODIFY*/

	return ret;
}	

/*
 * send decline packet
 */
int send_decline(unsigned long xid,unsigned long ciaddr,unsigned long server)
{
        int ret = 0;
        struct dhcpMessage packet;
      
        if(init_packet(&packet, DHCPDECLINE) < 0)
            return -1;
        
        packet.xid = random_xid();

        add_simple_option(packet.options, DHCP_REQUESTED_IP, ciaddr);
        add_simple_option(packet.options, DHCP_SERVER_ID, server);

	    if (add_requests(DHCPDECLINE, &packet) <0)
		    return -1;

        LOG(LOG_DEBUG, "Sending decline");

        ret = raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
                         SERVER_PORT, MAC_BCAST_ADDR, udhcp_client_config.ifindex);

/* ARRIS MODIFY*/
        udhcpc_mcb.plugin.convert_packet_to_log(&packet);
/* END ARRIS MODIFY*/

        return ret;
}


/* Unicasts a DHCP release message */
int send_release(unsigned long server, unsigned long ciaddr)
{
	struct dhcpMessage packet;
/* ARRIS MODIFY*/
        int ret = 0;
/* END ARRIS MODIFY*/

	if (init_packet(&packet,DHCPRELEASE) < 0) 
		return -1;

	packet.xid = random_xid();
	packet.ciaddr = ciaddr;
	
	add_simple_option(packet.options, DHCP_REQUESTED_IP, ciaddr);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	LOG(LOG_DEBUG, "Sending release...");
/* ARRIS MODIFY*/
	ret = kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT, udhcp_client_config.interface);

    udhcpc_mcb.plugin.convert_packet_to_log(&packet);
    return ret;
/* END ARRIS MODIFY*/

}

UDHCP_ERROR get_raw_packet(struct dhcpMessage *payload, int fd, int *len)
{
	int bytes;
	struct udp_dhcp_packet packet;
	u_int32_t source, dest;
	u_int16_t check;

	memset(&packet, 0, sizeof(struct udp_dhcp_packet));
	bytes = read(fd, &packet, sizeof(struct udp_dhcp_packet));
	if (bytes < 0) {
		LOG(LOG_WARNING,"couldn't read on raw listening socket -- ignoring");
		usleep(500000); /* possible down interface, looping condition */
		return UDHCP_SOCKET_ERROR;
	}
	
	if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
		LOG(LOG_WARNING,"message too short, ignoring");
		return UDHCP_SHORT_PKT_RECVD;
	}
	
	if (bytes < ntohs(packet.ip.tot_len)) {
		LOG( LOG_WARNING, "Truncated packet - ignoring");
		return UDHCP_TRUNCATED_PKT_RECVD;
	}
	
	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* Make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION ||
	    packet.ip.ihl != sizeof(packet.ip) >> 2 || packet.udp.dest != htons(CLIENT_PORT) ||
	    bytes > (int) sizeof(struct udp_dhcp_packet) ||
	    ntohs(packet.udp.len) != (short) (bytes - sizeof(packet.ip))) {
		//LOG( LOG_WARNING, "unrelated/bogus packet - ignoring");
	   	return UDHCP_BOGUS_PKT_RECVD;
	}

	/* check IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != checksum(&(packet.ip), sizeof(packet.ip))) {
		LOG( LOG_ERR, "bad IP header checksum - ignoring");
		return UDHCP_BAD_IP_CHKSUM_PKT_RECVD;
	}
	
	/* verify the UDP checksum by replacing the header with a psuedo header */
	source = packet.ip.saddr;
	dest = packet.ip.daddr;
	check = packet.udp.check;
	packet.udp.check = 0;
	memset(&packet.ip, 0, sizeof(packet.ip));

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source;
	packet.ip.daddr = dest;
	packet.ip.tot_len = packet.udp.len; /* cheat on the psuedo-header */
	if (check && check != checksum(&packet, bytes)) {
		LOG(LOG_ERR,"packet with bad UDP checksum received - ignoring expected: 0x%X calc: 0x%X ", check, checksum(&packet, bytes));
		return UDHCP_BAD_UDP_CHKSUM_PKT_RECVD;
	}
	
	memcpy(payload, &(packet.data), bytes - (sizeof(packet.ip) + sizeof(packet.udp)));
	
	if (ntohl(payload->cookie) != DHCP_MAGIC) {
		LOG(LOG_ERR,"received bogus message (bad magic) - ignoring");
		return UDHCP_BAD_DHCP_MAGIC_PKT_RECVD;
	}
	*len = (bytes - (sizeof(packet.ip) + sizeof(packet.udp)));
	return UDHCP_SUCCESS;
}

