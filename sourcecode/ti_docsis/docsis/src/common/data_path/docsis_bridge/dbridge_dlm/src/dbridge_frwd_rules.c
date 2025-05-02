/*
 *
 * dbridge_frwd_rules.c
 * Description:
 * DOCSIS bridge CM Forwarding Rules implementation
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#define DBRIDGE_FRWD_RULES_C

/*! \file dbridge_frwd_rules.c
*    \brief The file implements the CM Forwarding Rules:
*
*  1)CM Pre-Operational
*   - The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
*     TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
*     except the RF Interface;
*
*   - The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
*     HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*
*  2)CM Operational
*   - Forwarding of frames received from any CPE port conforms to the following specific rules:
*     The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
*     HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
*     for the purposes of configuration, secure software download, or address renewal.
*
*   - Forwarding of frames being sent by the CM IP stack conforms to the following specific rules:
*     The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
*     Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
*     except the RF port.
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/socket.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>

#include <net/ip.h>          /* ARRIS ADD */
#include <net/ipv6.h>       /* ARRIS ADD */

#include "dbridge_frwd_rules.h"


#define IPv4_VERSION    4
#define IPv6_VERSION    6                               

/* ARRIS ADD */
#define IP6F_OFF_MASK		0xfff8	/* mask out offset from ip6f_offlg */

/*Verify if next header is an external one*/
#define IPV6_EXT_HDR(hdr)  ((hdr == NEXTHDR_HOP)     || \
							(hdr == NEXTHDR_DEST)    || \
							(hdr == NEXTHDR_ROUTING) || \
							(hdr == NEXTHDR_FRAGMENT)|| \
							(hdr == NEXTHDR_AUTH)    || \
							(hdr == NEXTHDR_ESP)     || \
							(hdr == NEXTHDR_NONE))
/* ARRIS ADD END */

struct trhdr /* universal header for the transopt-layer protocol */
{
	__u16	src_port;
	__u16	dst_port;
};/*__attribute__((packed));*/    

/* ARRIS ADD */
static void ip6_skipExtHdrs(struct ipv6hdr *ipHdr, __u8 **upLayerHdr, __u8 *upLayerProto);
/* ARRIS ADD END */

/* TI_DHCPv4_DHCPDISCOVER, TI_DHCPv4_DHCPREQUEST, TI_TFTP_RRQ, TI_HTTP_REQUEST, TI_TIME_REQUEST */
static bool ip4_byDstPort( const struct sk_buff* skb, __u8 protocol, __u16 port);
/* TI_DHCPv6_SOLICIT, TI_DHCPv6_REQUEST, TI_TFTP_RRQ, TI_HTTP_REQUEST, TI_TIME_REQUEST */
static bool ip6_byDstPort( const struct sk_buff* skb, __u8 protocol, __u16 port);

/* TI_DHCPv4_DHCPOFFER, TI_DHCPv4_DHCPACK, TI_TFTP_DATA, TI_HTTP_RESPONSE, TI_TIME_RESPONSE */
static bool ip4_bySrcPort( const struct sk_buff* skb, __u8 protocol, __u16 port);
/* TI_DHCPv6_SOLICIT, TI_DHCPv6_REQUEST, TI_TFTP_RRQ, TI_HTTP_REQUEST, TI_TIME_REQUEST */
static bool ip6_bySrcPort( const struct sk_buff* skb, __u8 protocol, __u16 port);
/* TI_ROUTER_ADVERTISEMENTS, TI_ROUTER_SOLICITATION */
static bool ip6_icmp_msg(const struct sk_buff* skb, __u8 protocol, __u8 msg_type );

static inline  struct iphdr* ti_ip_hdr(const struct sk_buff* skb);
static inline  struct ipv6hdr* ip6_hdr(const struct sk_buff* skb);
static inline  struct trhdr* tr_hdr(const struct sk_buff* skb);
static inline  struct trhdr* tr6_hdr(const struct sk_buff* skb);

static inline __be16 eth_proto(const struct sk_buff* skb); /* => ETH_P_IP/ETH_P_IPV6 */

/* return value to the internet layer prtocol: IP, IPv6, ICMPv6, ..*/
static inline  __u8  ip4_version(const struct iphdr* iph);  
static inline  __u8  ip6_version(const struct ipv6hdr* iph);
static inline __u16  ip4_frag_offset(const struct iphdr* ip_hdr);                // ARRIS ADD
/* return value to the transport layer prtocol: IPPROTO_TCP, IPPROTO_UDP ..*/
static inline  __u8  ip4_protocol(const struct iphdr* iph); 
static inline  __u8  ip6_protocol(const struct ipv6hdr* iph);


/**************************************************************************/
/*      Help and debug functions                                          */
/**************************************************************************/
#if DEBUG_FLT  == 1
static char* proto2str(unsigned short h_proto);
static char* ver2str(unsigned char ip_version);
static char* protocol2str(unsigned char ip_protocol);
static void print_buf(const char* buf, int len, const char *title);
#endif

/**************************************************************************/
/*                      LOCAL DECLARATIONS:                               */
/**************************************************************************/


/********************************************************************/
/**************  get pointer to the header  *************************/
/********************************************************************/

/**************  link-layer headers  *************************/
/* 
 standart function: 
static inline  struct eth_hdr* eth_hdr(const struct sk_buff* skb)
*/

/**************  internet layer headers  *************************/

/**************************************************************************/
/*! \fn static inline  struct iphdr* ti_ip_hdr(const struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function return pointer to the ip header  
 *  \param[in] skb.
 *  \param[out] .
 *  \return pointer to the ip header.
**************************************************************************/
static inline  struct iphdr* ti_ip_hdr(const struct sk_buff* skb){
    return (struct iphdr*)(skb_mac_header(skb) + sizeof(struct ethhdr));
}
/**************************************************************************/
/*! \fn static inline  struct ipv6hdr* ip6_hdr(const struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function return pointer to the ipv6 header  
 *  \param[in] skb.
 *  \param[out] .
 *  \return pointer to the ipv6 header.
**************************************************************************/
static inline  struct ipv6hdr* ip6_hdr(const struct sk_buff* skb){
    return (struct ipv6hdr*)(skb_mac_header(skb) + sizeof(struct ethhdr));
}

/******************  transport layer headers  *********************/

/**************************************************************************/
/*! \fn static inline  struct trhdr* tr_hdr(const struct sk_buff* skb)
 **************************************************************************
 *  \brief The function return pointer to the universal transport layer header  
 *  \param[in] skb.
 *  \param[out] .
 *  \return pointer to the header.
**************************************************************************/
static inline  struct trhdr* tr_hdr(const struct sk_buff* skb){
     return (struct trhdr*)( ((unsigned char *)ti_ip_hdr(skb)) + sizeof(__u32)*( (ti_ip_hdr(skb))->ihl & 0x0F) ); 
}
/**************************************************************************/
/*! \fn static inline  struct trhdr* tr6_hdr(const struct sk_buff* skb)
 **************************************************************************
 *  \brief The function return pointer to the universal transport layer header  
 *  \param[in] skb.
 *  \param[out] .
 *  \return pointer to the header.
**************************************************************************/
static inline  struct trhdr* tr6_hdr(const struct sk_buff* skb){ 
    return (struct trhdr*)( ((unsigned char *)ti_ip_hdr(skb)) + sizeof(struct ipv6hdr) ); 
}

/********************************************************************/
/**************  get field from the header  *************************/
/********************************************************************/

/******************  link-layer data  *********************/

/**************************************************************************/
/*! \fn static inline  __be16 eth_proto(const struct sk_buff* skb)
 **************************************************************************
 *  \brief The function return value of the link-layer prtocol: ETH_P_IP/ETH_P_IPV6 ..  
 *  \param[in] skb.
 *  \param[out] .
 *  \return pointer to the header.
**************************************************************************/
static inline  __be16 eth_proto(const struct sk_buff* skb){
     return ntohs((eth_hdr(skb))->h_proto);
}                                          

/************** inetwork layer data *************************/

/**************************************************************************/
/*! \fn static inline __u8 ip4_version(const struct iphdr* ip_hdr)
 **************************************************************************
 *  \brief The function return ip version of the protocol - v4, v6 
 *  \param[in] struct iphdr* ip_hdr - pointer to the ipv4 header.
 *  \param[out] .
 *  \return ip version.
**************************************************************************/
static inline __u8 ip4_version(const struct iphdr* ip_hdr){
     return ip_hdr->version;
}

// ARRIS ADD
/**************************************************************************/
/*! \fn static inline __u8 ip4_frag_offset(const struct iphdr* ip_hdr)
 **************************************************************************
 *  \brief The function returns the frag_offset of the protocol - v4
 *  \param[in] struct iphdr* ip_hdr - pointer to the ipv4 header.
 *  \param[out] .
 *  \return ip frag_offset.
**************************************************************************/
static inline __u16 ip4_frag_offset(const struct iphdr* ip_hdr){
     return ip_hdr->frag_off;
}
// END ARRIS ADD

/**************************************************************************/
/*! \fn static inline __u8 ip4_protocol(const struct iphdr* ip_hdr)
 **************************************************************************
 *  \brief The function return type of the transport layer protocol - 
 *          IPPROTO_TCP, IPPROTO_UDP 
 *  \param[in] struct iphdr* ip_hdr - pointer to the ipv4 header.
 *  \param[out] .
 *  \return ip version.
**************************************************************************/
static inline __u8 ip4_protocol(const struct iphdr* ip_hdr){
     return ip_hdr->protocol;
}
/**************************************************************************/
/*! \fn static inline __u8 ip6_version(const struct ipv6hdr* ip_hdr)
 **************************************************************************
 *  \brief The function return ip version of the protocol - v4, v6 
 *  \param[in] struct ipv6hdr* ip_hdr - pointer to the ipv6 header.
 *  \param[out] .
 *  \return ip version.
**************************************************************************/
static inline __u8 ip6_version(const struct ipv6hdr* ip_hdr){
     return ip_hdr->version;
}
/**************************************************************************/
/*! \fn static inline __u8 ip6_protocol(const struct ipv6hdr* ip_hdr)
 **************************************************************************
 *  \brief The function return type of the transport layer protocol - 
 *          IPPROTO_TCP, IPPROTO_UDP 
 *  \param[in] struct ipv6hdr* ip_hdr - pointer to the ipv6 header.
 *  \param[out] .
 *  \return ip version.
**************************************************************************/
static inline __u8 ip6_protocol(const struct ipv6hdr* ip_hdr){
     return ip_hdr->nexthdr;
}


/* ARRIS ADD */
/* 
 * When fragment don't contain up layer header, upLayerHdr will ouput NULL.
*/
static void ip6_skipExtHdrs(struct ipv6hdr *ipHdr, __u8 **upLayerHdr, __u8 *upLayerProto)
{
    struct ipv6_opt_hdr *hdr;
    struct frag_hdr *fhdr;
    __u8 nexthdr = ipHdr->nexthdr;
    __u32 offset = sizeof(struct ipv6hdr);
    __u32 hdrlen;
    __u8 fragOffset = false;
      
    while (IPV6_EXT_HDR(nexthdr))
    {           
        /*If this is the last next header*/
        if (nexthdr == NEXTHDR_NONE) 
        {
            break;
        }

        /*Encrypted header - cannot parse it, treat as uknown header */
        if (nexthdr == NEXTHDR_ESP) 
        {
            break;
        }
        
        hdr = (struct ipv6_opt_hdr*)((__u8 *)ipHdr + offset);
        if (nexthdr == NEXTHDR_FRAGMENT)
        {
            hdrlen = sizeof(struct frag_hdr);
            
            fhdr = (struct frag_hdr *)hdr;
            if (fhdr->frag_off & htons(IP6F_OFF_MASK))
            {
                fragOffset = true;
            }
        }
        else if (nexthdr == NEXTHDR_AUTH)
        {
            hdrlen = (hdr->hdrlen+2)<<2;
        }
        else
        {
            hdrlen = ipv6_optlen(hdr);
        }
        
        offset += hdrlen;
        nexthdr = hdr->nexthdr;
    }

    if (!fragOffset)
    {
        *upLayerHdr = (__u8 *)ipHdr + offset;
        *upLayerProto = nexthdr;
    }
    else
    {
        *upLayerHdr = NULL;
        *upLayerProto = nexthdr;
    }

    return;
}
/* ARRIS ADD END */

/******************  transport layer data  *********************/

/**************************************************************************/
/*! \fn static inline  __u16 src_port(const struct trhdr* tr_hdr)
 **************************************************************************
 *  \brief The function return source port of the packet 
 *  \param[in] struct trhdr* tr_hdr - pointer to the universal header of the 
 *       transport layer protocol.
 *  \param[out] .
 *  \return source port.
**************************************************************************/
static inline  __u16 src_port(const struct trhdr* tr_hdr){
     return ntohs(tr_hdr->src_port);
}
/**************************************************************************/
/*! \fn static inline  __u16 dst_port(const struct trhdr* tr_hdr)
 **************************************************************************
 *  \brief The function return destination port of the packet 
 *  \param[in] struct trhdr* tr_hdr - pointer to the universal header of the 
 *       transport layer protocol.
 *  \param[out] .
 *  \return destination port.
**************************************************************************/
static inline  __u16 dst_port(const struct trhdr* tr_hdr){
     return ntohs(tr_hdr->dst_port);
}

/**************************************************************************/
/*! \fn static bool ip4_byDstPort(const struct sk_buff* skb, __u8 protocol, __u16 port)
 **************************************************************************
 *  \brief check the packet by destination port.
 *      network layer = IP 
 *  \param[in] const struct sk_buff* skb.
 *  \param[in] __u8 protocol - ip protocol.
 *  \param[in] __u16 port - the port port of the packet.
 *  \return true or false
 **************************************************************************/
               
static bool ip4_byDstPort(const struct sk_buff* skb, __u8 protocol, __u16 port ){
    struct iphdr* iph = ti_ip_hdr(skb);

    if( eth_proto(skb) == ETH_P_IP && 
       ip4_version(iph) == IPv4_VERSION && ip4_protocol(iph) == protocol && 
       !(ip4_frag_offset(iph) & htons(IP_OFFSET)) &&   /* ARRIS ADD - Only apply to IP fragments that will contain headers */
       dst_port(tr_hdr(skb)) == port)
    {
        D_PRINTK(("\n[%s]: match > ETH_P_IP/IPv4_VERSION/%s/%04d\n", __func__, protocol2str(protocol), port))
        return true;
    }
    return false;

}

/**************************************************************************/
/*! \fn static bool ip4_bySrcPort(const struct sk_buff* skb, __u8 protocol, __u16 port)
 **************************************************************************
 *  \brief check the packet by source port
 *      network layer = IP 
 *  \param[in] const struct sk_buff* skb.
 *  \param[in] __u8 protocol - ip protocol.
 *  \param[in] __u16 port - the source port of the packet
 *  \return true or false
 **************************************************************************/
 
static bool ip4_bySrcPort(const struct sk_buff* skb, __u8 protocol, __u16 port ){
    struct iphdr* iph = ti_ip_hdr(skb);

    if( eth_proto(skb) == ETH_P_IP && 
        ip4_version(iph) == IPv4_VERSION && ip4_protocol(iph) == protocol &&
        !(ip4_frag_offset(iph) & htons(IP_OFFSET)) &&   /* ARRIS ADD - Only apply to IP fragments that will contain headers */
        src_port(tr_hdr(skb)) == port )
    {
        D_PRINTK(("\n[%s]: match > ETH_P_IP/IPv4_VERSION/%s/%04d\n", __func__, protocol2str(protocol), port))
        return true;
    }
    return false;
}
/**************************************************************************/
/*! \fn static bool ip6_byDstPort(const struct sk_buff* skb, __u8 protocol, __u16 port)
 **************************************************************************
 *  \brief check the packet by destination port.
 *      network layer = IP 
 *  \param[in] const struct sk_buff* skb.
 *  \param[in] __u8 protocol - ip protocol.
 *  \param[in] __u16 port - the destination port of the packet.
 *  \return true or false
 **************************************************************************/
               
static bool ip6_byDstPort(const struct sk_buff* skb, __u8 protocol, __u16 port ){
    /* ARRIS MOD START */
    struct ipv6hdr* iph = ip6_hdr(skb);
    struct trhdr* upLayerHdr;
    __u8 proto;
    
    if ((eth_proto(skb) == ETH_P_IPV6) && 
        (ip6_version(iph) == IPv6_VERSION))
    {
        ip6_skipExtHdrs(iph, &upLayerHdr, &proto);
        if (upLayerHdr)
        {
            if ((proto==protocol) && (dst_port(upLayerHdr) == port))
            {
                D_PRINTK(("\n[%s]: match > ETH_P_IPV6/IPv6_VERSION/%s/%04d\n", __func__, protocol2str(protocol), port))
                return true;
            }
        }
    }
    /* ARRIS MOD END */
    
    return false;
}

/**************************************************************************/
/*! \fn static bool ip6_bySrcPort(const struct sk_buff* skb, __u8 protocol, __u16 port)
 **************************************************************************
 *  \brief check the packet by source port.
 *      network layer = IP 
 *  \param[in] const struct sk_buff* skb.
 *  \param[in] __u8 protocol - ip protocol.
 *  \param[in] __u16 port - the source port of the packet.
 *  \return true or false
 **************************************************************************/
 
static bool ip6_bySrcPort(const struct sk_buff* skb, __u8 protocol, __u16 port ){
    /* ARRIS MOD START */
    struct ipv6hdr* iph = ip6_hdr(skb);
    struct trhdr* upLayerHdr;
    __u8 proto;
    
    if ((eth_proto(skb) == ETH_P_IPV6) && 
        (ip6_version(iph) == IPv6_VERSION))
    {
        ip6_skipExtHdrs(iph, &upLayerHdr, &proto);
        if (upLayerHdr)
        {
            if ((proto==protocol) && (src_port(upLayerHdr) == port))
            {
                D_PRINTK(("\n[%s]: match > ETH_P_IPV6/IPv6_VERSION/%s/%04d\n", __func__, protocol2str(protocol), port))
                return true;
            }
        }
    }
    /* ARRIS MOD END */
    
    return false;
}


/**************************************************************************/
/*! \fn static bool ip6_icmp(const struct sk_buff* skb,  __u8 protocol, __u16 port)
 **************************************************************************
 *  \brief check if this is a router_advertisements or router_solicitation packet
 *        msg type 133/ICMPv6/IPv6 , msg type 134/ICMPv6/IPv6
 *  \param[in] const struct sk_buff* skb.
 *  \param[in] __u8 protocol - ip protocol(icmpv6 = 58).
 *  \param[in] __u16 msg_type - router_advertisements = 134
 *                              router_solicitation = 133.
 *  \return true or false
 **************************************************************************/
               
static bool ip6_icmp_msg(const struct sk_buff* skb, __u8 protocol, __u8 msg_type ){
    /* ARRIS MOD START */
    struct ipv6hdr* iph = ip6_hdr(skb);
    struct icmp6hdr* upLayerHdr;
    __u8 proto;
    
    if ((eth_proto(skb) == ETH_P_IPV6) && 
        (ip6_version(iph) == IPv6_VERSION))
    {
        ip6_skipExtHdrs(iph, &upLayerHdr, &proto);
        if (upLayerHdr)
        {
            if ((proto==protocol) && (upLayerHdr->icmp6_type == msg_type))
            {
                D_PRINTK(("\n[%s]: match > ETH_P_IPV6/IPv6_VERSION/%s/%02d\n", __func__, protocol2str(protocol), msg_type))
                return true;
            }
        }
    }
    /* ARRIS MOD END */    

    return false;
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/**************************************************************************/
/*! \fn static bool dhcpv4_cli2srv(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if the dhcpv4 discover/request packet. The dhcpv4 discover/request packet is 
 *         a message from the client(src port 68) to the server(dst port 67).
 *    dst 67/UDP/IP or src 68/UDP/IP 
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
 
static bool dhcpv4_cli2srv(const struct sk_buff* skb){
    return ip4_byDstPort(skb, IPPROTO_UDP, 67);
}
/**************************************************************************/
/*! \fn static bool dhcpv6_cli2srv(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if the dhcpv6 solicit/request packet. The dhcpv6 solicit/request  
 *    packet is  a message from the client(src port 546) to the server(dst port 547).
 *    dst 547/UDP/IPv6 or src 546/UDP/IPv6 
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/

static bool dhcpv6_cli2srv(const struct sk_buff* skb){
    return ip6_byDstPort(skb, IPPROTO_UDP, 547);
}

/**************************************************************************/
/*! \fn static bool tftp_rrq(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a tftp read-request packet. The packet sent to
 *          the server.
 *        dst 69/UDP/IP; dst 69/UDP/IPv6
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool tftp_rrq(const struct sk_buff* skb){
    return ( ip4_byDstPort(skb, IPPROTO_UDP, 69) || ip6_byDstPort(skb, IPPROTO_UDP, 69));
}
/**************************************************************************/
/*! \fn static bool http_request(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a http-request packet. The packet sent to
 *          the server(dst port - 80/TCP, 8080/TCP).
 *      dst 80(8080)/TCP/IP; dst 80(8080)/TCP/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool http_request(const struct sk_buff* skb){
    return ( ip4_byDstPort(skb, IPPROTO_TCP, 80) || ip6_byDstPort(skb, IPPROTO_TCP, 80) ||
             ip4_byDstPort(skb, IPPROTO_TCP, 8080) || ip6_byDstPort(skb, IPPROTO_TCP, 8080));
}
/**************************************************************************/
/*! \fn static bool time_request(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a ToD request packet. The packet sent to
 *          the server(dst port - 37/UDP).
 *      dst 37/UDP/IP; dst 37/UDP/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool time_request(const struct sk_buff* skb){
    return ( ip4_byDstPort(skb, IPPROTO_UDP, 37) || ip6_byDstPort(skb, IPPROTO_UDP, 37));
}
/**************************************************************************/
/*! \fn static bool router_solicitation(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a router_solicitation message. 
 *      msg type 133/ICMPv6/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool router_solicitation(const struct sk_buff* skb){
    return ip6_icmp_msg(skb, IPPROTO_ICMPV6, 133 );
}

/**************************************************************************/
/**************************************************************************/

/**************************************************************************/
/*! \fn static bool dhcpv4_srv2cli(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if the dhcpv4 offer/ack packet.The offer/dhcpv4 ack packet is 
 *         a message from the server(src port 67) to the  client(dst port 68).
 *      src 67/UDP/IP or dst 68/UDP/IP 
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/

static bool dhcpv4_srv2cli(const struct sk_buff* skb){
    return ip4_bySrcPort(skb, IPPROTO_UDP, 67);
}
/**************************************************************************/
/*! \fn static bool dhcpv6_srv2cli(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if the dhcpv6 advertise/replay packet. The dhcpv6 advertise/replay packet is 
 *         a message from the server(src port 547) to the client(dst port 546).
 *    src 547/UDP/IPv6 or dst 546/UDP/IPv6 
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/

static bool dhcpv6_srv2cli(const struct sk_buff* skb){
    return ip6_bySrcPort(skb, IPPROTO_UDP, 547);
}

/**************************************************************************/
/*! \fn static bool tftp_data(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a tftp request data packet. The packet sent from
 *          the server to the client.
 *        src 69/UDP/IP; src 69/UDP/IPv6
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool tftp_data(const struct sk_buff* skb){
    return ( ip4_bySrcPort(skb, IPPROTO_UDP, 69) || ip6_bySrcPort(skb, IPPROTO_UDP, 69) );
}
/**************************************************************************/
/*! \fn static bool http_response(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a http-response packet. The packet sent from
 *          the server(src port - 80/8080) to the client.
 *      src 80(8080)/TCP/IP; src 80(8080)/TCP/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool http_response(const struct sk_buff* skb){
    return ( ip4_bySrcPort(skb, IPPROTO_TCP, 80) || ip6_bySrcPort(skb, IPPROTO_TCP, 80) ||
             ip4_bySrcPort(skb, IPPROTO_TCP, 8080) || ip6_bySrcPort(skb, IPPROTO_TCP, 8080));
}
/**************************************************************************/
/*! \fn static bool time_response(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a ToD response packet. The packet sent from
 *          the server(src port - 37/UDP).
 *      dst 37/UDP/IP; dst 37/UDP/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool time_response(const struct sk_buff* skb){
    return ( ip4_bySrcPort(skb, IPPROTO_UDP, 37) || ip6_bySrcPort(skb, IPPROTO_UDP, 37));
}


/**************************************************************************/
/*! \fn static bool router_advertisements(const struct sk_buff* skb)
 **************************************************************************
 *  \brief check if this is a router_advertisements message. 
 *      msg type 134/ICMPv6/IPv6  
 *  \param[in] const struct sk_buff* skb.
 *  \return true or false
 **************************************************************************/
static bool router_advertisements(const struct sk_buff* skb){
    return ip6_icmp_msg(skb, IPPROTO_ICMPV6, 134 );
}
/*****************************/

static const struct fwrd_rule frule[] = {
  /*--- IP stack to RF only  ---*/
  {  TI_DHCPv4_DHCPDISCOVER,   dhcpv4_cli2srv },       /* srv-dst 67/UDP/IP or cli-src 68/UDP/IP */
  {  TI_DHCPv4_DHCPREQUEST ,   dhcpv4_cli2srv  },      /* srv-dst 67/UDP/IP or cli-src 68/UDP/IP */ 
  {  TI_DHCPv6_SOLICIT,        dhcpv6_cli2srv      },  /* srv-dst 547/UDP/IPv6 or cli-src 546/UDP/IPv6*/
  {  TI_DHCPv6_REQUEST,        dhcpv6_cli2srv      },  /* srv-dst 547/UDP/IPv6 or cli-src 546/UDP/IPv6 */
  {  TI_TFTP_RRQ,              tftp_rrq },             /* srv-dst 69/UDP/IP; srv-dst 69/UDP/IPv6 */           
  {  TI_HTTP_REQUEST,          http_request },         /* srv-dst 80(8080)/TCP/IP; srv-dst 80(8080)/TCP/IPv6 */         
  {  TI_TIME_REQUEST,          time_request },         /* srv-dst 37/UDP/IP; srv-dst 37/UDP/IPv6 */         
  {  TI_ROUTER_SOLICITATION,   router_solicitation},   /* msg type 133/ICMPv6/IPv6 */
  /*-   -- not accept from CPE  ---*/                        
  {  TI_DHCPv4_DHCPOFFER,      dhcpv4_srv2cli },       /* srv-src 67/UDP/IP or cli-dst 68/UDP/IP */ 
  {  TI_DHCPv4_DHCPACK,        dhcpv4_srv2cli },       /* srv-src 67/UDP/IP or cli-dst 68/UDP/IP */ 
  {  TI_DHCPv6_ADVERTISE,      dhcpv6_srv2cli },       /* srv-src 547/UDP/IPv6 or cli-dst 546/UDP/IPv6 */ 
  {  TI_DHCPv6_REPLY,          dhcpv6_srv2cli },       /* srv-src 547/UDP/IPv6 or cli-dst 546/UDP/IPv6 */ 
  {  TI_TFTP_DATA,             tftp_data },            /* src 69/UDP/IP; src 69/UDP/IPv6 */ 
  {  TI_HTTP_RESPONSE,         http_response },        /* src 80(8080)/TCP/IP; src 80(8080)/TCP/IPv6 */ 
  {  TI_TIME_RESPONSE,         time_response },        /* src 37/UDP/IP; src 37/UDP/IPv6 */        
  {  TI_ROUTER_ADVERTISEMENTS, router_advertisements } /* msg type 134/ICMPv6/IPv6 */
};
/**************************************************************************/
/*! \fn bool apply_fwrd_rule(const struct fwrd_rule *rules, unsigned int opt_code,  
 *             unsigned int rule_first, unsigned int rule_last, const struct sk_buff *skb)
 **************************************************************************
 *  \brief  The function checks the input packet according to the given rules.
 *     If the packet is macth to the on of rules the function return true,
 *     otherwise - false. The search process is continuing to the first match.
 *     The function will be search between the "rule_first" and "rule_last". This values
 *     taking from the structure which keeps all rules("struct fwrd_rule *rules")
 *     The input data arraived in the "struct sk_buff *skb".
 * 
 *  \param[in] const struct fwrd_rule *rules - pointer to the array of the rules
 *  \param[in] unsigned int opt_code - option code(bit mask) of the rule or combination of the 
 *                 rules for the look for
 *  \param[in] unsigned int rule_first_index - start index of the rules array for the search.
 *  \param[in] unsigned int rule_last_index - last index in the rules array for the search.
 *             The search includes the last rule. 
 *  \param[in] const struct sk_buff *skb
 *  \return true or false
 **************************************************************************/

bool apply_fwrd_rule(const struct fwrd_rule *rules, unsigned int opt_code,  
                    unsigned int rule_first_index, unsigned int rule_last_index, const struct sk_buff *skb)
{
    int i;
    __be16		h_proto;

    if( NULL == skb )                                      return false;
    D_PRINTK(( "\t%s(IN):h_proto = 0x%04x \t",__FUNCTION__, (unsigned int)eth_proto(skb)));
    /* The filter for work with IP and IPv6 packets. If you want to catch any other packets
     - you need to close this filter */
    h_proto = eth_proto(skb);
    if ( h_proto != ETH_P_IP && h_proto != ETH_P_IPV6 )    return false;

    if( NULL == rules )    rules = frule; 
    if( rule_first_index > rule_last_index )   rule_last_index = TI_ROUTER_ADVERTISEMENTS_IND;

    D_PRINTK(( "\t%s(to loop)\t",__FUNCTION__));

    for( i=rule_first_index; opt_code && i<=rule_last_index; i++ )
    {
       D_PRINTK(( "%d, ", i ))
       if ( (opt_code & rules[i].opt_code) && rules[i].do_rule(skb) ) {
            D_PRINTK(( "\n\t%s(OUT) -  index = %d; return true\n", __FUNCTION__,i ))
            return true;
       }
       opt_code &= ~rules[i].opt_code;
    }
    D_PRINTK(( "\n\t%s(OUT) - return false\n", __FUNCTION__ ))
    
    return false;
}

/**************************************************************************
 *                           DEBUG FUNCTIONS
 ***************************************************************************/

#if DEBUG_FLT  == 1

static char* proto2str(unsigned short h_proto)
{
    static char *p;
    switch ( h_proto )
    {
        case ETH_P_IP:      p = "ETH_P_IP";   break;  
        case ETH_P_LOOP:    p = "ETH_P_LOOP"; break;
        case ETH_P_ARP:     p = "ETH_P_ARP"; break;
        case ETH_P_IPV6:    p = "ETH_P_IPV6"; break;
        case ETH_P_802_2:   p = "ETH_P_802_2"; break;
        case ETH_P_802_3:   p = "ETH_P_802_3"; break;
        default:            p = "UNKNOWN";    break;
    }

    return p;
}

static char* ver2str(unsigned char ip_version)
{
    static char *p;
    switch ( ip_version )
    {
        case AF_UNSPEC:      p = "AF_UNSPEC";   break;  
        case IPv4_VERSION/*AF_INET*/:    p = "IPv4_VERSION"; break;
        case IPv6_VERSION/*AF_INET6*/:   p = "IPv6_VERSION"; break;
        default:            p = "UNKNOWN";    break;
    }
    return p;
}

static char* protocol2str(unsigned char ip_protocol)
{
    static char *p;
    switch ( ip_protocol )
    {
        case IPPROTO_IP:      p = "IPPROTO_IP";   break;  
        case IPPROTO_ICMP:    p = "IPPROTO_ICMP"; break;
        case IPPROTO_IGMP:     p = "IPPROTO_IGMP"; break;
        case IPPROTO_TCP:    p = "IPPROTO_TCP"; break;
        case IPPROTO_EGP:   p = "IPPROTO_EGP"; break;
        case IPPROTO_UDP:   p = "IPPROTO_UDP"; break;
        case IPPROTO_IPV6:   p = "IPPROTO_IPV6"; break;
        case IPPROTO_SCTP:   p = "IPPROTO_SCTP"; break;
        case IPPROTO_RAW:   p = "IPPROTO_RAW"; break;
        case IPPROTO_ICMPV6:   p = "IPPROTO_ICMPV6"; break; 
        default:            p = "UNKNOWN";    break;
    }
    return p;
}


static void print_buf(const char* buf, int len, const char *title)
{
    int i;
    if(title != NULL)   printk( "%s",  title);
    else                  printk( "buf: " );
    for(i=0; i<len;i++)    printk( "%02x ", buf[i] );
    printk( "\n" );
}

#endif  /* DEBUG_FLT  == 1 */

