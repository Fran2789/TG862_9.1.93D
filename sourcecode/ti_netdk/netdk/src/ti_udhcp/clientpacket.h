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

#ifndef _CLIENTPACKET_H
#define _CLIENTPACKET_H

unsigned long random_xid(void);
int send_discover(unsigned long xid, unsigned long requested);
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_decline(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_release(unsigned long server, unsigned long ciaddr);
UDHCP_ERROR get_raw_packet(struct dhcpMessage *payload, int fd, int *len);

#endif
