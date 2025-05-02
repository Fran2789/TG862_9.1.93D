/* file udhcp_packet.h ; renamed packet.h */
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
// 3. Expended UDHCP_ERROR return value.
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

#ifndef _UDHCP_PACKET_H
#define _UDHCP_PACKET_H

#include <netinet/udp.h>
#include <netinet/ip.h>
#include "udhcp_cfg.h"

#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNAK			6
#define DHCPRELEASE		7
#define DHCPINFORM		8

/* DHCP protocol -- see RFC 2131 */
#define SERVER_PORT		67
#define CLIENT_PORT		68

#define DHCP_MAGIC		0x63825363

/* DHCP option codes (partial list) */
#define DHCP_PADDING		0x00
#define DHCP_SUBNET		0x01
#define DHCP_TIME_OFFSET	0x02
#define DHCP_ROUTER		0x03
#define DHCP_TIME_SERVER	0x04
#define DHCP_NAME_SERVER	0x05
#define DHCP_DNS_SERVER		0x06
#define DHCP_LOG_SERVER		0x07
#define DHCP_COOKIE_SERVER	0x08
#define DHCP_LPR_SERVER		0x09
#define DHCP_HOST_NAME		0x0c
#define DHCP_BOOT_SIZE		0x0d
#define DHCP_DOMAIN_NAME	0x0f
#define DHCP_SWAP_SERVER	0x10
#define DHCP_ROOT_PATH		0x11
#define DHCP_IP_TTL		0x17
#define DHCP_MTU		0x1a
#define DHCP_BROADCAST		0x1c
#define DHCP_NTP_SERVER		0x2a
#define DHCP_WINS_SERVER	0x2c
#define DHCP_REQUESTED_IP	0x32
#define DHCP_LEASE_TIME		0x33
#define DHCP_OPTION_OVER	0x34
#define DHCP_MESSAGE_TYPE	0x35
#define DHCP_SERVER_ID		0x36
#define DHCP_PARAM_REQ		0x37
#define DHCP_MESSAGE		0x38
#define DHCP_MAX_SIZE		0x39
#define DHCP_T1			0x3a
#define DHCP_T2			0x3b
#define DHCP_VENDOR		0x3c
#define DHCP_CLIENT_ID		0x3d
#define DHCP_BOOT_FILE		0x43   /* UNIHAN ADD */



#define DHCP_VENDOR_SPEC    0x2b    /*SERCOMM ADD*/


#define DHCP_END		0xFF
#define BROADCAST_FLAG		0x8000
#define ETH_10MB		1
#define ETH_10MB_LEN		6

#define BOOTREQUEST		1
#define BOOTREPLY		2
#define OPTION_FIELD		0
#define FILE_FIELD		1
#define SNAME_FIELD		2
#define MAC_BCAST_ADDR		(unsigned char *) "\xff\xff\xff\xff\xff\xff"



struct dhcpMessage {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	u_int32_t ciaddr;
	u_int32_t yiaddr;
	u_int32_t siaddr;
	u_int32_t giaddr;
	u_int8_t chaddr[16];
	u_int8_t sname[64];
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE]; /* 4 bytes cookie + options = total options */ 
};

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
};

/* UDHCP error codes used by report_config() */
typedef enum UDHCP_ERROR
{
	UDHCP_SUCCESS, /* =0 */
	UDHCP_RFC_VIOLATION, /* If there is a RFC violation in the received packet */
	UDHCP_MAX_CONFIG_ATTEMPTS_REACHED, /* If Client attempted MAX_CONFIG_ATTEMPTS times
										   to obtain configuration from server 
										*/
	UDHCP_PLUGIN_REJECTED_OPTION, /* Option is rejected by Plugin */
	UDHCP_LEASE_LOST, /* Lease lost */
	UDHCP_SHORT_PKT_RECVD, /*  packet too short */
	UDHCP_TRUNCATED_PKT_RECVD, /* packet truncated */
	UDHCP_BOGUS_PKT_RECVD, /* packet bogus - IP version incorrect, port incorrect etc */
	UDHCP_BAD_IP_CHKSUM_PKT_RECVD, /* IP checksum error */
	UDHCP_BAD_UDP_CHKSUM_PKT_RECVD, /* UDP checksum error */
	UDHCP_BAD_DHCP_MAGIC_PKT_RECVD, /* DHCP Magic number error */
	UDHCP_SOCKET_ERROR,  /* socket read error */	
	UDHCP_INTERNAL_ERROR, /* Error internal to Core */
    UDHCP_INTERNAL_SAVE_AND_WAIT, /* packet is valid, keep it and wait for a better offer till timeout*/
	UDHCP_ERROR_LAST, 
} UDHCP_ERROR;

char *udhcp_get_error_str(UDHCP_ERROR error);

#endif
