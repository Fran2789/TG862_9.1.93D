/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
//  1. The server_config structure is extended to an array to define dhcp server 
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

#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
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
#include <errno.h>

#include "udhcp_packet.h"
#include "udhcp_debug.h"
#include "options.h"

char *udhcp_error_texts[] = { 
          "success",
          "RFC violation",
          "Max config attempt reached",
          "Options rejected by plugin",
          "DHCPv4 current lease lost",
          "short packet received",
          "truncated packet received",
          "bugus packet received",
          "packet with wrong IP Checksum received",
          "packet with wrong UDP Checksum received",
          "packet with wrong DHCP Magic number received",
          "socket read error",
          "internal error" };

void init_header(struct dhcpMessage *packet, char type)
{
    memset(packet, 0, sizeof(struct dhcpMessage));
    switch (type) {
    case DHCPDISCOVER:
    case DHCPREQUEST:
    case DHCPRELEASE:
    case DHCPINFORM:
        packet->op = BOOTREQUEST;
        break;
    case DHCPOFFER:
    case DHCPACK:
    case DHCPNAK:
        packet->op = BOOTREPLY;
    }
    packet->htype = ETH_10MB;
    packet->hlen = ETH_10MB_LEN;
    packet->cookie = htonl(DHCP_MAGIC);
    packet->options[0] = DHCP_END;
    add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
}
char *udhcp_get_error_str(UDHCP_ERROR error) 
{ 
    return udhcp_error_texts[error]; 
}

/* read a packet from socket fd, return -1 on read error, -2 on packet error */
UDHCP_ERROR get_packet(struct dhcpMessage *packet, int fd, int *len)
{
    int bytes;
    int i;
    const char broken_vendors[][8] = {
        "MSFT 98",
        ""
    };
    char unsigned *vendor;

    *len = 0;
    memset(packet, 0, sizeof(struct dhcpMessage));
    bytes = read(fd, packet, sizeof(struct dhcpMessage));
    if (bytes < 0) {
        LOG(LOG_ERR, "couldn't read on listening socket, ignoring");
        return UDHCP_SOCKET_ERROR;
    }

    if (ntohl(packet->cookie) != DHCP_MAGIC) {
        LOG(LOG_ERR, "received bogus message, ignoring");
        return UDHCP_BAD_DHCP_MAGIC_PKT_RECVD;
    }
    
    LOG(LOG_DEBUG, "Received a packet");
    if (packet->op == BOOTREQUEST && (vendor = get_option(packet, DHCP_VENDOR, len))) {
        for (i = 0; broken_vendors[i][0]; i++) {
            if (vendor[OPT_LEN - 2] == (unsigned char) strlen(broken_vendors[i]) &&
                !strncmp(vendor, broken_vendors[i], vendor[OPT_LEN - 2])) {
                    LOG(LOG_INFO, "broken client (%s), forcing broadcast",
                        broken_vendors[i]);
                    packet->flags |= htons(BROADCAST_FLAG);
            }
        }
    }
    *len = bytes;
    return UDHCP_SUCCESS;
}


u_int16_t checksum(void *addr, int count)
{
    /* Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     */
    register int32_t sum = 0;
    u_int16_t *source = (u_int16_t *) addr;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += *source++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 ) {
                /* Make sure that the left-over byte is added correctly both
                 * with little and big endian hosts */
                u_int16_t tmp = 0;
                *(u_int8_t *) (&tmp) = * (u_int8_t *) source;
                sum += tmp;
        //sum += * (unsigned char *) source;
    //LOG(LOG_WARNING, "Count = %d sum = %u *source: % u  typecasted source: %u ", count, sum, *source, *(unsigned char *) source);
    }

    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}


/* Constuct a ip/udp header for a packet, and specify the source and dest hardware address */
int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
           u_int32_t dest_ip, int dest_port, unsigned char *dest_arp, int ifindex)
{
    int fd;
    int result;
    struct sockaddr_ll dest;
    struct udp_dhcp_packet packet;
    int optionlen_wopad;
    int end = end_option(payload->options);

    // ARRIS ADD : Handle buffer check failure
    if (end < 0) {
        LOG(LOG_ERR, "raw_packet: Buffer overrun looking for end option");
        return -1;
    }
    // ARRIS END

    if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
        DEBUG(LOG_ERR, "socket call failed: %s", sys_errlist[errno]);
        return -1;
    }
    
    memset(&dest, 0, sizeof(dest));
    memset(&packet, 0, sizeof(packet));
    
    dest.sll_family = AF_PACKET;
    dest.sll_protocol = htons(ETH_P_IP);
    dest.sll_ifindex = ifindex;
    dest.sll_halen = 6;
    memcpy(dest.sll_addr, dest_arp, 6);
    if (bind(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_ll)) < 0) {
        DEBUG(LOG_ERR, "bind call failed: %s", sys_errlist[errno]);
        close(fd);
        return -1;
    }

    packet.ip.protocol = IPPROTO_UDP;
    packet.ip.saddr = source_ip;
    packet.ip.daddr = dest_ip;
    packet.udp.source = htons(source_port);
    packet.udp.dest = htons(dest_port);

    /* The options buffer is a fixed length array of CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE bytes.
       Instead of sending the whole CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE bytes including padding 
       (present after end of all the options), strip the padding and send only the options set by client/plugin.
    */
    optionlen_wopad = CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE - end - 1;

    packet.udp.len = htons(sizeof(packet.udp) + sizeof(struct dhcpMessage) - optionlen_wopad);
    packet.ip.tot_len = packet.udp.len;
    memcpy(&(packet.data), payload, sizeof(struct dhcpMessage) - optionlen_wopad);
    packet.udp.check = checksum(&packet, sizeof(struct udp_dhcp_packet) - optionlen_wopad);

    packet.ip.tot_len = htons(sizeof(struct udp_dhcp_packet) - optionlen_wopad);
    packet.ip.ihl = sizeof(packet.ip) >> 2;
    packet.ip.version = IPVERSION;
    packet.ip.ttl = IPDEFTTL;
    packet.ip.check = checksum(&(packet.ip), sizeof(packet.ip));
    result = sendto(fd, &packet, sizeof(struct udp_dhcp_packet) - optionlen_wopad, 0, (struct sockaddr *) &dest, sizeof(dest));
    if (result <= 0) {
        DEBUG(LOG_ERR, "write on socket failed: %s", sys_errlist[errno]);
    }

    close(fd);
    return result;
}


/* Let the kernel do all the work for packet generation */
int kernel_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
           u_int32_t dest_ip, int dest_port, char *interface)
{
    int n = 1;
    int fd, result;
    struct sockaddr_in client;
    int optionlen_wopad;
    int end = end_option(payload->options);
    
    // ARRIS ADD : Handle buffer check failure
    if (end < 0) {
        LOG(LOG_ERR, "kernel_packet: Buffer overrun looking for end option");
        return -1;
    }
    // ARRIS END

    if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return -1;
    
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) == -1)
        return -1;

    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(source_port);
    client.sin_addr.s_addr = source_ip;

    if (bind(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
        return -1;

    /* make sure the raw packet goes out wfrom the correct interface */
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,interface, strlen(interface) + 1) == -1)
        return -1;

    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(dest_port);
    client.sin_addr.s_addr = dest_ip; 

    if (connect(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
        return -1;

    /* The options buffer is a fixed length array of CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE bytes.
       Instead of sending the whole CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE bytes including padding 
       (present after end of all the options), strip the padding and send only the options set by client/plugin.
    */
    optionlen_wopad = CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE - end - 1;

    result = write(fd, payload, sizeof(struct dhcpMessage) - optionlen_wopad);

    close(fd);
    return result;
}   
