#ifndef _SERVERPACKET_H
#define _SERVERPACKET_H

/* Change Description:07112006
 * 1. Option 43 handling added in init_packet
 * 2. Modified functions to include classindex to indicate correct server_config
 */

int sendOffer(struct dhcpMessage *oldpacket, int ifid,int classindex,int flag43);
int sendNAK(struct dhcpMessage *oldpacket, int ifid,int classindex,int flag43);
int sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr, int ifid,int classindex,int flag43);
int send_inform(struct dhcpMessage *oldpacket, int ifid,int classindex,int flag43);


#endif
