/*
 *
 * dbridge_snif.c
 * Description:
 * DOCSIS bridge sniffer implementation
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

#define _DBRIDGE_SNIF_C_

/*! \file dbridge_snif.c
    \brief the docsis bridge sniffer module
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/icmp.h>
#include <linux/igmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/route.h>

#include <linux/netfilter.h>
//#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/jiffies.h>         // ARRIS ADD
#include <linux/ipv6.h>            // ARRIS ADD
  
/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define SNIF_MAC_LEN 14
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

static DEFINE_SPINLOCK(log_lock);

 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static unsigned char st[120];

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/




static void sniffUnknown(unsigned char* addr) 
{
   printk("Unknown %s", st);
}

static void sniffArp(unsigned char* addr)  {
   #pragma pack(1)
   struct  arp_header {
      unsigned short htype;
      unsigned short ptype;
      unsigned char  hlen;
      unsigned char  plen;
      unsigned short op;
      unsigned char data;
   } *arp;
   #pragma pack()
   int i;
   int j;
   char tmp[80];

   arp = (struct arp_header*) addr;
   switch(arp->op)   {
     // ARRIS CHANGE - Replace strcat with strcpy to prevent text corruption from reused st buffer
      case 1: strcpy(st,"ARP request)"); break;
      case 2: strcpy(st,"ARP response)"); break;
      case 3: strcpy(st,"RARP request)"); break;
      case 4: strcpy(st,"RARP response)"); break;
      default: strcpy(st,"?)"); break;
      // END ARRIS CHANGE
   }
   printk("%s ", st);
   printk("Htype: %04X, Ptype: %04X ",
               arp->htype, arp->ptype);
   j=0;
   strcpy(st,"Sender: ");
   for(i=0; i<(arp->hlen - 1); i++) {
      sprintf(tmp,"%02X-",(&arp->data)[j]);
      strcat(st,tmp);
      j++;
   }
   sprintf(tmp,"%02X   IP: ",(&arp->data)[j]);
   strcat(st,tmp);
   j++;
   for(i=0; i<(arp->plen - 1); i++) {
      sprintf(tmp,"%d.",(&arp->data)[j]);
      strcat(st,tmp);
      j++;
   }
   sprintf(tmp,"%d",(&arp->data)[j]);
   strcat(st,tmp);
   printk("%s ", st);

   strcpy(st,"Target: ");
   j++;
   for(i=0; i<(arp->hlen - 1); i++) {
      sprintf(tmp,"%02X-",(&arp->data)[j]);
      strcat(st,tmp);
      j++;
   }
   sprintf(tmp,"%02X   IP: ",(&arp->data)[j]);
   strcat(st,tmp);
   j++;
   for(i=0; i<(arp->plen - 1); i++) {
      sprintf(tmp,"%d.",(&arp->data)[j]);
      strcat(st,tmp);
      j++;
   }
   sprintf(tmp,"%d",(&arp->data)[j]);
   strcat(st,tmp);
   printk("%s ", st);
   j++;
}


static char *icmp2str(char typ) {
   switch (typ) {
   case 0:
      return ("ECHO Reply");
      break;
   case 3:
      return ("DEST Unreachable");
      break;
   case 8:
      return ("ECHO Request");
      break;
   default:
      return ("Other");
   }

}

//ARRIS ADD
static char *icmpv62str(char typ) {
   switch (typ) {
   case 1:
      return ("DEST Unreachable");
      break;
   case 2:
      return ("Packet Too Big");
      break;
   case 3:
      return ("Time Exceeded");
      break;
   case 4:
      return ("Parameter Problem");
      break;
   case 128:
       return ("Echo Request");
       break;
   case 129:
      return ("Echo Reply");
      break;
   default:
      return ("Other");
   }
}
//ARRIS END ADD

static char *igmp2str(char typ) {
	switch (typ) 
		{
	case 0x11:      
		return("IGMP Membership Query");  
		break;            
	case 0x12:     
		return("IGMPv1 Membership Report");  
		break;              
	case 0x13:     
		return("DVMRP");  
		break;                            
	case 0x14:     
		return("PIM version 1");  
		break;                            
	case 0x15:     
		return("Cisco Trace Messages");  
		break; 

	case 0x16:     
		return("IGMPv2 Membership Report");  
		break;             

	case 0x17:     
		return("IGMPv2 Leave Group");  
		break;                   

	case 0x1e:     
		return("Multicast Traceroute Response");  
		break;        

	case 0x1f:     
		return("Multicast Traceroute");  
		break;                

	case 0x22:     
		return("IGMPv3 Membership Report");  
		break;          
	case 0x30:     
		return("Multicast Router Advertisement");  
		break;         

	case 0x31:     
		return("Multicast Router Solicitation");  
		break;           

	case 0x32:     
		return("Multicast Router Termination");  
		break;           
	default:  	
		return("Reserved for experimentation");  
		break;      
	}
}

static char *prot2str(char prot) {

   switch (prot) {
   case 1:
      return ("ICMP");
      break;
   case 2:
      return ("IGMP");
      break;
   case 6:
      return ("TCP");
      break;
   case 17:
      return ("UDP");
      break;
   default:
      return ("Other");
   }

}

static char *port2str(short port) {
   switch (port) {
   case 37:
      return ("TIME");
      break;
   case 20:
      return ("FTP-Data");
      break;
   case 21:
      return ("FTP");
      break;
   case 67:
   case 68:
      return ("BOOTP");
      break;
   case 69:
      return ("TFTP");
      break;
   case 161:
   case 162:
      return ("SNMP");
      break;
   default:
      return ("Other");
   }
}

static void sniffIp(unsigned char* addr)   {
   unsigned char hlen;
#pragma pack(1)
    struct  ip_header {
      unsigned char  ver;
      unsigned char  tos;
      unsigned short len;
      unsigned short ident;
      unsigned short freg;
      unsigned char  ttl;
      unsigned char  proto;
      unsigned short csum;
      unsigned char  sip1;
      unsigned char  sip2;
      unsigned char  sip3;
      unsigned char  sip4;
      unsigned char  dip1;
      unsigned char  dip2;
      unsigned char  dip3;
      unsigned char  dip4;
   } *eth;

   struct  icmp_header {
      unsigned char  typ;
      unsigned char  cod;
      unsigned short cs;
      unsigned char  data;
   } *icmp;

   struct  udp_header {
      unsigned short sport;
      unsigned short dport;
      unsigned short mlen;
      unsigned short cs;
      unsigned char  data;
   } *udp = NULL;

   struct  tcp_header {
      unsigned short sport;
      unsigned short dport;
      unsigned long  seqn;
      unsigned long  ackn;
      unsigned short hlen;
      unsigned short win;
      unsigned short cs;
      unsigned short urgp;
   } *tcp;

   struct dhcp_header {
       unsigned char op;
       unsigned char htype;
       unsigned char hlen;
       unsigned char hops;
       unsigned long xid;
       unsigned short secs;
       unsigned short flags;
       unsigned long  ciaddr;
       unsigned long  yiaddr;
       unsigned long  siaddr;
       unsigned long  giaddr;
       unsigned char  chaddr01;
       unsigned char  chaddr02;
       unsigned char  chaddr03;
       unsigned char  chaddr04;
       unsigned char  chaddr05;
       unsigned char  chaddr06;
   }*dhcp;
#pragma pack()
   struct igmphdr *igmp_hdr; 
   unsigned char *pdata = NULL;

   eth = (struct ip_header*) addr;
   hlen = (eth->ver & 0x0F) << 2;
   

   printk("IP header: Ver: %02X, HLen: %d, TOS: %02X, Length %d ",
            (eth->ver & 0xF0) >>4,(eth->ver & 0xF), eth->tos, eth->len );
   printk("Ident: %04X, Fragment: %04X ", eth->ident, eth->freg);
   printk("TTL: %02X, Protocl: %d (%s), HCSUM: %04X ",
            eth->ttl, eth->proto, prot2str(eth->proto), eth->csum);
   printk("IP SA: %d.%d.%d.%d, IP DA: %d.%d.%d.%d ",
            eth->sip1, eth->sip2, eth->sip3, eth->sip4,eth->dip1, eth->dip2,
            eth->dip3, eth->dip4);

   switch (eth->proto) {
   case 1: /* ICMP */
      icmp = (struct icmp_header*) ((unsigned char *)eth+hlen);
      printk("ICMP: Type: %d (%s), Code: %d ", icmp->typ,
               icmp2str(icmp->typ), icmp->cod);
      pdata = &(icmp->data);
      break;
   case 2: /* IGMP  */
       igmp_hdr = (struct igmphdr *) ((unsigned char *)eth+hlen);
	   printk ("IGMP Type 0x%X (%s),Group = %4X ", igmp_hdr->type,
			   igmp2str(igmp_hdr->type), igmp_hdr->group);
      break;
   case 6: /* TCP */
      tcp = (struct tcp_header*) ((unsigned char *)eth+hlen);
      printk("TCP: Source port: %d (%s), Dest port: %d (%s) ", tcp->sport,
               port2str(tcp->sport), tcp->dport, port2str(tcp->dport));
      hlen = (tcp->hlen & 0xF000) >> 10;
      pdata = (unsigned char*) ((unsigned char *)tcp+hlen);
      break;
   case 17: /* UDP */
      udp = (struct udp_header*) ((unsigned char *)eth+hlen);
// ARRIS MODIFY  1 line - add time stamp
      printk("UDP: Time: %lu, Source port: %d (%s), Dest port: %d (%s) ", jiffies, udp->sport,
               port2str(udp->sport), udp->dport, port2str(udp->dport));
      printk("Len: %d, CheckSum: %04X ", udp->mlen, udp->cs);
      pdata = &(udp->data);
      break;
   }
   if (pdata != NULL)
   {
       if (17 == eth->proto && 
           (68==udp->dport || 67==udp->dport)) /*DHCP*/
       {
           dhcp = (struct dhcp_header*)pdata;
           printk("\nDHCP: chaddr=%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
                  dhcp->chaddr01, dhcp->chaddr02, dhcp->chaddr03, dhcp->chaddr04,
                  dhcp->chaddr05, dhcp->chaddr06);
       }
       else
       {
           printk("Data: %02X %02X %02X %02X %02X %02X %02X %02X ...",
              pdata[0],pdata[1],pdata[2],pdata[3],
              pdata[4],pdata[5],pdata[6],pdata[7]);
       }
   }
}


//ARRIS ADD
static void sniffIpv6(unsigned char* addr)   
{
    unsigned char hlen;
    struct ipv6hdr *eth;

#pragma pack(1)
    struct  icmp_header {
        unsigned char  typ;
        unsigned char  cod;
        unsigned short cs;
        unsigned char  data;
    } *icmp;

    struct  udp_header {
        unsigned short sport;
        unsigned short dport;
        unsigned short mlen;
        unsigned short cs;
        unsigned char  data;
   } *udp;

   struct  tcp_header {
       unsigned short sport;
       unsigned short dport;
       unsigned long  seqn;
       unsigned long  ackn;
       unsigned short hlen;
       unsigned short win;
       unsigned short cs;
       unsigned short urgp;
    } *tcp;

    struct dhcp_header {
        unsigned char op;
        unsigned char htype;
        unsigned char hlen;
        unsigned char hops;
        unsigned long xid;
        unsigned short secs;
        unsigned short flags;
        unsigned long  ciaddr;
        unsigned long  yiaddr;
        unsigned long  siaddr;
        unsigned long  giaddr;
        unsigned char  chaddr01;
        unsigned char  chaddr02;
        unsigned char  chaddr03;
        unsigned char  chaddr04;
        unsigned char  chaddr05;
        unsigned char  chaddr06;
    }*dhcp;
#pragma pack()

    struct igmphdr *igmp_hdr; 
    unsigned char *pdata = NULL;

    eth = (struct ipv6hdr *) addr;

    printk("IPV6 header: Ver: %02X, Length: %d, TTL: %02X, Protocol: %d\n", 
           eth->version, eth->payload_len, eth->hop_limit, eth->nexthdr );

    printk("IPv6 SA: %X:%X:%X:%X:%X:%X:%X:%X\n", eth->saddr.in6_u.u6_addr16[0],eth->saddr.in6_u.u6_addr16[1],
           eth->saddr.in6_u.u6_addr16[2],eth->saddr.in6_u.u6_addr16[3],
           eth->saddr.in6_u.u6_addr16[4],eth->saddr.in6_u.u6_addr16[5],
           eth->saddr.in6_u.u6_addr16[6],eth->saddr.in6_u.u6_addr16[7]);

    printk("IPv6 DA: %X:%X:%X:%X:%X:%X:%X:%X\n", eth->daddr.in6_u.u6_addr16[0],eth->daddr.in6_u.u6_addr16[1],
           eth->daddr.in6_u.u6_addr16[2],eth->daddr.in6_u.u6_addr16[3],
           eth->daddr.in6_u.u6_addr16[4],eth->daddr.in6_u.u6_addr16[5],
           eth->daddr.in6_u.u6_addr16[6],eth->daddr.in6_u.u6_addr16[7]);

    hlen = sizeof(struct ipv6hdr);

    switch (eth->nexthdr) 
    {
        case 1: /* ICMP */
            icmp = (struct icmp_header*) ((unsigned char *)eth+hlen);
            printk("ICMP: Type: %d (%s), Code: %d ", icmp->typ, icmp2str(icmp->typ), icmp->cod);
            pdata = &(icmp->data);
            break;
        case 2: /* IGMP  */
            igmp_hdr = (struct igmphdr *) ((unsigned char *)eth+hlen);
            printk ("IGMP Type 0x%X (%s),Group = %4X ", igmp_hdr->type, igmp2str(igmp_hdr->type), igmp_hdr->group);
            break;
        case 6: /* TCP */
            tcp = (struct tcp_header*) ((unsigned char *)eth+hlen);
            printk("TCP: Source port: %d (%s), Dest port: %d (%s) ", tcp->sport, port2str(tcp->sport), tcp->dport, port2str(tcp->dport));
            hlen = (tcp->hlen & 0xF000) >> 10;
            pdata = (unsigned char*) ((unsigned char *)tcp+hlen);
            break;
        case 17: /* UDP */
            udp = (struct udp_header*) ((unsigned char *)eth+hlen);
            printk("UDP: Time: %lu, Source port: %d (%s), Dest port: %d (%s) ", jiffies, udp->sport,
                   port2str(udp->sport), udp->dport, port2str(udp->dport));
            printk("Len: %d, CheckSum: %04X ", udp->mlen, udp->cs);
            pdata = &(udp->data);
            break;
        case 58: /* ICMPv6 */
            icmp = (struct icmp_header*) ((unsigned char *)eth+hlen);
            printk("ICMP: Type: %d (%s), Code: %d ", icmp->typ, icmpv62str(icmp->typ), icmp->cod);
            pdata = &(icmp->data);
            break;
    }

    if( pdata != NULL )
    {
        if( (17 == eth->nexthdr) && (547==udp->dport || 546==udp->dport)) /*DHCP*/ // ARRIS MOD updated ports to v6 values
        {
            dhcp = (struct dhcp_header*)pdata;
            printk("\nDHCP: chaddr=%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
                   dhcp->chaddr01, dhcp->chaddr02, dhcp->chaddr03, dhcp->chaddr04,
                   dhcp->chaddr05, dhcp->chaddr06);
        }
        else
        {
            printk("Data: %02X %02X %02X %02X %02X %02X %02X %02X ...",
                   pdata[0],pdata[1],pdata[2],pdata[3],
                   pdata[4],pdata[5],pdata[6],pdata[7]);
        }
    }
}
//ARRIS ADD END

/**************************************************************************/
/*! \fn void DBridge_logPacket(const struct sk_buff *skb)
 **************************************************************************
 *  \brief print the packet from SKB 
 *  \param[in] skb.
 *  \return None
 **************************************************************************/
void DBridge_logPacket(const struct sk_buff *skb)
	{
    if ( !skb )
    {
            return;
        }

        spin_lock_bh(&log_lock);

		if(skb->dev && skb->dev->hard_header_len == SNIF_MAC_LEN)
		{
           struct ethhdr*     ptr_ethhdr;
        ptr_ethhdr = eth_hdr(skb);
        if (ptr_ethhdr)
        {

           printk("\nSRC MAC: %.2x-%.2x-%.2x-%.2x-%.2x-%.2x DEST MAC: %.2x-%.2x-%.2x-%.2x-%.2x-%.2x PROT %.4x\n",
		   (unsigned char)ptr_ethhdr->h_source[0],
		   (unsigned char)ptr_ethhdr->h_source[1], 
		   (unsigned char)ptr_ethhdr->h_source[2],
		   (unsigned char)ptr_ethhdr->h_source[3],
		   (unsigned char)ptr_ethhdr->h_source[4],
		   (unsigned char)ptr_ethhdr->h_source[5],
		   (unsigned char)ptr_ethhdr->h_dest[0],
		   (unsigned char)ptr_ethhdr->h_dest[1], 
		   (unsigned char)ptr_ethhdr->h_dest[2],
		   (unsigned char)ptr_ethhdr->h_dest[3],
		   (unsigned char)ptr_ethhdr->h_dest[4],
		   (unsigned char)ptr_ethhdr->h_dest[5],
		   (unsigned short)ptr_ethhdr->h_proto);


            switch (ptr_ethhdr->h_proto)
            {
			  case 0x800:
				 sniffIp((skb->data)+SNIF_MAC_LEN);
				 break;
			  case 0x806:
				 sniffArp((skb->data)+SNIF_MAC_LEN);
				 break;
//ARRIS ADD
          case 0x86DD:
            sniffIpv6((skb->data)+SNIF_MAC_LEN);
            break;
//ARRIS END ADD
			  default:
				 sniffUnknown((skb->data)+SNIF_MAC_LEN);
				 break;
			}
		   printk("\n");
        }

		}
		else
		{
		
        if (skb->dev && 
            skb->dev->hard_header_len && 
            (skb_mac_header(skb) != skb_network_header(skb)))
        {
            int i;
            const unsigned char *p = skb_mac_header(skb);
		printk("MAC=");
			for (i = 0; i < skb->dev->hard_header_len; i++,p++)
            {
                printk("%02x%c", *p, i==skb->dev->hard_header_len - 1 ? ' ':':');
            }
			printk("\n");
		}
		
		}

		spin_unlock_bh(&log_lock);

		return;
}
