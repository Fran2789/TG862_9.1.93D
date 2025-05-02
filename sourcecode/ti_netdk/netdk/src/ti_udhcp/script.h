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

#ifndef _SCRIPT_H
#define _SCRIPT_H
char **fill_envp(struct dhcpMessage *packet);

#endif
