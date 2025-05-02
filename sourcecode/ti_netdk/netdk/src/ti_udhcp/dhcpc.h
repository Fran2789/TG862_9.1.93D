/* dhcpc.h */
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

#ifndef _DHCPC_H
#define _DHCPC_H

#include "udhcp_cfg.h"
#include "udhcpc_plugin.h"

//#define TI_UDHCPC_VERSION "1.0"

#define UPTIME_FILE_PATH        "/proc/uptime"
#define MAX_LINE_SIZE           64


struct in6_pktinfo {
        struct in6_addr ipi6_addr;
        int             ipi6_ifindex;
        };

extern struct udhcp_client_config_t udhcp_client_config;
extern UDHCPC_PACKET_STATS udhcpc_stats;

typedef struct udhcp_client_config_t 
{
	char foreground;		/* Do not fork */
	char quit_after_lease;		/* Quit after obtaining lease */
	char abort_if_no_lease;		/* Abort if no lease */
	unsigned char arp[6];		/* Our arp address */
	char *interface;		/* The name of the interface to use */
	unsigned char *clientid;	/* Optional client id to use */
	unsigned char *hostname;	/* Optional hostname to use */
	int ifindex;			/* Index number of the interface to use */
	unsigned long lease;
	unsigned int backoff_time;
} udhcp_client_config_t;


typedef struct UDHCPC_MCB
{
	UDHCPC_PLUGIN_MCB plugin;
	unsigned short major_version;
	unsigned short minor_version;
} UDHCPC_MCB;

extern UDHCPC_MCB udhcpc_mcb;
extern udhcpc_client_state udhcp_get_client_state(void);
#endif
