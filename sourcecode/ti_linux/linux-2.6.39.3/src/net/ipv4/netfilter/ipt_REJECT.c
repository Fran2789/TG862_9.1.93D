/*
 * This is a module which is used for rejecting packets.
 */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/route.h>
#include <net/dst.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_REJECT.h>
#ifdef CONFIG_BRIDGE_NETFILTER
#include <linux/netfilter_bridge.h>
#endif
/*SERCOMM ADD*/
#include <linux/syscalls.h>

#define BLOCKPAGE_PATH               "/var/tmp/block.htm"
#define BLOCKPAGE_HTTP_HEADER        "HTTP/1.0 200 OK\r\nServer: Router\r\nConten-Type:text/html\r\n\r\n"
#define BLOCKPAGE_DEFAULT            "<!doctype html><html><head><meta charset=\"UTF-8\"><title>Web Site Blocked</title></head><body style=\"background:#ffffff;\"><div style=\"padding: 70px 40px; width:680px;\"><svg width=\"60px\" style=\"float:left; margin:0px 15px 0 0\" height=\"60px\" version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\" viewBox=\"0 0 595.3 595.3\" enable-background=\"new 0 0 595.3 595.3\" xml:space=\"preserve\"><path fill=\"#CC0022\" d=\"M274,79.8c10.5-18.2,36.7-18.2,47.2,0L439.1,284L557,488.2c10.5,18.2-2.6,40.9-23.6,40.9H297.6H61.8  c-21,0-34.1-22.7-23.6-40.9L156.1,284L274,79.8z\"></path></svg>   <div style=\"font-size: 34px; padding: 20px 0px 0px 70px; position:relative; color:#CC0022;\"><span style=\"position:absolute; color:#fff; left:3.6%; top:25%;\">!</span> This website is not allowed</div><br><div style=\"font-size: 22px;\">Parental control is blocking the content of this website from viewing.</div></div></body></html>"
#define BLOCKPAGE_MAX_LEN            1024
static char HttpContent[BLOCKPAGE_MAX_LEN] = {0};
static int  HttpContent_len = 0;
/*SERCOMM ADD END*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("Xtables: packet \"rejection\" target for IPv4");

/*SERCOMM ADD*/
static unsigned int kread_file(char *fname, unsigned int length, char *ptr)
{
    unsigned int bytesRead;
    int fd;
    mm_segment_t fs = get_fs();
    set_fs(get_ds());

    fd = sys_open(fname, 0, 0);
    if(fd == -1) {
        return -1;
    }
    bytesRead = sys_read(fd, ptr, length);

    sys_close(fd);
    set_fs(fs);

    return bytesRead;
}

static void send_block(struct sk_buff *oldskb, int hook)
{
    struct sk_buff *nskb;
    const struct iphdr *oiph;
    struct iphdr *niph;
    const struct tcphdr *oth;
    struct tcphdr _otcph, *tcph;
    char *http;
    
    /* IP header checks: fragment. */
    if (ip_hdr(oldskb)->frag_off & htons(IP_OFFSET))
    {
        return;
    }
    
    oth = skb_header_pointer(oldskb, ip_hdrlen(oldskb),sizeof(_otcph), &_otcph);
    if (oth == NULL)
    {
        return;
    }
    
    /* Check checksum */
    if (nf_ip_checksum(oldskb, hook, ip_hdrlen(oldskb), IPPROTO_TCP))
    {
        return;
    }
    
    nskb = alloc_skb(sizeof(struct iphdr) + sizeof(struct tcphdr) + LL_MAX_HEADER + HttpContent_len, GFP_ATOMIC);
    if (!nskb)
    {
        return;
    }
    skb_reserve(nskb, LL_MAX_HEADER);
    
    oiph = ip_hdr(oldskb);
    skb_reset_network_header(nskb);
    niph = (struct iphdr *)skb_put(nskb, sizeof(struct iphdr));
    niph->version   = 4;
    niph->ihl       = sizeof(struct iphdr) / 4;
    niph->tos       = 0;
    niph->id        = 0;
    niph->frag_off  = htons(IP_DF);
    niph->protocol  = IPPROTO_TCP;
    niph->check     = 0;
    niph->saddr     = oiph->daddr;
    niph->daddr     = oiph->saddr;
    niph->ttl       = MAXTTL;
    //niph->ttl       = ip4_dst_hoplimit(skb_dst(nskb));

    tcph = (struct tcphdr *)skb_put(nskb, sizeof(struct tcphdr));
    memset(tcph, 0, sizeof(*tcph));
    tcph->source    = oth->dest;
    tcph->dest      = oth->source;
    tcph->doff      = sizeof(struct tcphdr) / 4;
    tcph->fin       = 1;
    tcph->psh       = 1;
    tcph->ack       = 1;
    tcph->seq = oth->ack_seq;
    tcph->ack_seq = htonl(ntohl(oth->seq) + oth->syn + oth->fin + oldskb->len - ip_hdrlen(oldskb) - (oth->doff << 2));

    skb_trim(nskb, niph->ihl*4+sizeof(struct tcphdr));
    http = skb_put(nskb, HttpContent_len);
    strncpy(http, HttpContent, HttpContent_len);
    niph->tot_len = htons(nskb->len);
    
    tcph->check = ~tcp_v4_check(sizeof(struct tcphdr)+HttpContent_len, niph->saddr, niph->daddr, 0);
    nskb->ip_summed = CHECKSUM_PARTIAL;
    nskb->csum_start = (unsigned char *)tcph - nskb->head;
    nskb->csum_offset = offsetof(struct tcphdr, check);
    
    /* ip_route_me_harder expects skb->dst to be set */
    skb_dst_set_noref(nskb, skb_dst(oldskb));

    nskb->protocol = htons(ETH_P_IP);
    if (ip_route_me_harder(nskb, RTN_UNSPEC))
    {
        goto free_nskb;
    }

    /* "Never happens" */
    if (nskb->len > dst_mtu(skb_dst(nskb)))
    {
        goto free_nskb;
    }
    
    nf_ct_attach(nskb, oldskb);

    ip_local_out(nskb);
    return;

 free_nskb:
    kfree_skb(nskb);
}
/*SERCOMM ADD END */

// ARRIS ADD - Start
struct dns_head {
	uint16_t id;
	uint16_t flags;
	uint16_t nquer;
	uint16_t nansw;
	uint16_t nauth;
	uint16_t nadd;
};
struct type_and_class {
	uint16_t type;
	uint16_t class;
} PACKED;

#define REQ_A 1
#define REQ_PTR 12
#define MAX_PACK_LEN 512
#define move_from_unaligned16(v, u16p) (memcpy(&(v), (u16p), 2))
#define move_from_unaligned32(v, u32p) (memcpy(&(v), (u32p), 4))
# define move_to_unaligned16(u16p, v) do { \
	uint16_t __t = (v); \
	memcpy((u16p), &__t, 4); \
} while (0)
# define move_to_unaligned32(u32p, v) do { \
	uint32_t __t = (v); \
	memcpy((u32p), &__t, 4); \
} while (0)

#define PUTSHORT(s, cp) { \
	u16 t_s = (u16)(s); \
	unsigned char *t_cp = (unsigned char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += 2; \
}

// Process dns packet and answer with dummy IP
static int process_dns_packet(uint8_t *buf)
{
	struct dns_head *head;
	struct type_and_class *unaligned_type_class;
	char *query_string;
	uint8_t *answb;
	uint16_t outr_rlen;
	uint16_t outr_flags;
	uint16_t type;
	uint16_t class;
	int query_len;
	char *ans;
	uint32_t ip;

	printk("process_packet(): buf len =  %d \n", strlen(buf));
	head = (struct dns_head *)buf;
	if (head->nquer == 0) {
		printk("process_packet(): error 1 \n");
		return 0; /* don't reply */
	}
	if (head->flags & htons(0x8000)) { /* QR bit */
		printk("process_packet(): error 2 \n");
		return 0; /* don't reply */
	}
	/* QR = 1 "response", RCODE = 4 "Not Implemented" */
	outr_flags = htons(0x8000 | 4);

	/* start of query string */
	query_string = (void *)(head + 1);
	/* caller guarantees strlen is <= MAX_PACK_LEN */
	query_len = strlen(query_string) + 1;
	/* may be unaligned! */
	unaligned_type_class = (void *)(query_string + query_len);
	query_len += sizeof(*unaligned_type_class);
	/* where to append answer block */
	answb = (void *)(unaligned_type_class + 1);

	/* OPCODE != 0 "standard query"? */
	if ((head->flags & htons(0x7800)) != 0) {
		printk("process_packet(): error 3 \n");
		goto empty_packet;
	}
	move_from_unaligned16(class, &unaligned_type_class->class);
	if (class != htons(1)) { /* not class INET? */
		printk("process_packet(): error 4 \n");
		goto empty_packet;
	}
	move_from_unaligned16(type, &unaligned_type_class->type);
	if (type != htons(REQ_A) && type != htons(REQ_PTR)) {
		/* we can't handle this query type */
		printk("process_packet(): error 5 \n");
		goto empty_packet;
	}

	ip = 0xC0A87B7B;
	ans = &ip;
	outr_rlen = 4;
	if (ans && type == htons(REQ_PTR)) {
		/* returning a host name */
		outr_rlen = strlen(ans) + 1;
	}

	if (!ans
	 || (unsigned)(answb - buf) + query_len + 4 + 2 + outr_rlen > MAX_PACK_LEN
	) {
		/* QR = 1 "response"
		 * AA = 1 "Authoritative Answer"
		 * RCODE = 3 "Name Error" */
		//err_msg = "name is not found";
		outr_flags = htons(0x8000 | 0x0400 | 3);
		goto empty_packet;
	}

	/* Append answer Resource Record */
	//memcpy(answb, query_string, query_len); /* name, type, class */
	//answb += query_len;
	PUTSHORT(sizeof(struct dns_head) | 0xc000, answb);
	PUTSHORT(type, answb);
	PUTSHORT(class, answb);
	move_to_unaligned32((uint32_t *)answb, htonl(0));
	answb += 4;
	move_to_unaligned16((uint16_t *)answb, htons(outr_rlen));
	answb += 2;
	memcpy(answb, ans, outr_rlen);
	answb += outr_rlen;

	/* QR = 1 "response",
	 * AA = 1 "Authoritative Answer",
	 * TODO: need to set RA bit 0x80? One user says nslookup complains
	 * "Got recursion not available from SERVER, trying next server"
	 * "** server can't find HOSTNAME"
	 * RCODE = 0 "success"
	 */
	outr_flags = htons(0x8000 | 0x0400 | 0);
	//outr_flags = htons(0x8000 | 0x0400 | 0x0080 | 0);
	/* we have one answer */
	head->nansw = htons(1);

 empty_packet:
	head->flags |= outr_flags;
	head->nauth = head->nadd = 0;
	head->nquer = htons(1); // why???

	printk("process_packet(): error 6 \n");
	return answb - buf;
}

static void send_dns_answer(struct sk_buff *oldskb, int hook)
{
    struct sk_buff *nskb;
    const struct iphdr *oiph;
    struct iphdr *niph;
    const struct udphdr *ouh;
    struct udphdr _oudph, *udph;
    char *dnsbuf;
    uint8_t buf[MAX_PACK_LEN + 1];
    int pr_return;
    
    /* IP header checks: fragment. */
    if (ip_hdr(oldskb)->frag_off & htons(IP_OFFSET))
    {
	printk("send_dns_answer(): IP header check fails\n");
        return;
    }
    
    ouh = skb_header_pointer(oldskb, ip_hdrlen(oldskb),sizeof(_oudph), &_oudph);
    if (ouh == NULL)
    {
	printk("send_dns_answer(): ouh is null\n");
        return;
    }
    
    /* Check checksum */
    if (nf_ip_checksum(oldskb, hook, ip_hdrlen(oldskb), IPPROTO_UDP))
    {
	printk("send_dns_answer(): checksum fails\n");
        return;
    }

    memset(buf, 0, sizeof(buf));
    skb_copy_bits(oldskb, ip_hdrlen(oldskb) + sizeof(struct udphdr), buf, (oldskb->len - ip_hdrlen(oldskb) - sizeof(struct udphdr)));
    
    pr_return = process_dns_packet(buf);
	    
    nskb = alloc_skb(sizeof(struct iphdr) + sizeof(struct udphdr) + LL_MAX_HEADER + pr_return, GFP_ATOMIC);
    if (!nskb)
    {
	printk("send_dns_answer(): nskb null \n");
        return;
    }
    skb_reserve(nskb, LL_MAX_HEADER);
    
    oiph = ip_hdr(oldskb);
    skb_reset_network_header(nskb);
    niph = (struct iphdr *)skb_put(nskb, sizeof(struct iphdr));
    niph->version   = 4;
    niph->ihl       = sizeof(struct iphdr) / 4;
    niph->tos       = 0;
    niph->id        = 0;
    niph->frag_off  = htons(IP_DF);
    niph->protocol  = IPPROTO_UDP;
    niph->check     = 0;
    niph->saddr     = oiph->daddr;
    niph->daddr     = oiph->saddr;
    niph->ttl       = MAXTTL;

    udph = (struct udphdr *)skb_put(nskb, sizeof(struct udphdr));
    memset(udph, 0, sizeof(*udph));
    udph->source    = ouh->dest;
    udph->dest      = ouh->source;
    udph->len       = sizeof(struct udphdr) + pr_return;

    skb_trim(nskb, niph->ihl*4+sizeof(struct udphdr));
    dnsbuf = skb_put(nskb, pr_return);
    memcpy(dnsbuf, buf, pr_return);
    niph->tot_len = htons(nskb->len);
    
    //udph->check = csum_tcpudp_magic(niph->saddr, niph->daddr, sizeof(struct udphdr)+pr_return, IPPROTO_UDP, csum_partial((char*)udph, sizeof(struct udphdr)+pr_return, 0));
    udph->check = ~csum_tcpudp_magic(niph->saddr, niph->daddr, sizeof(struct udphdr)+pr_return, IPPROTO_UDP, 0);
    nskb->ip_summed = CHECKSUM_PARTIAL;
    nskb->csum_start = (unsigned char *)udph - nskb->head;
    nskb->csum_offset = offsetof(struct udphdr, check);
    
    /* ip_route_me_harder expects skb->dst to be set */
    skb_dst_set_noref(nskb, skb_dst(oldskb));

    nskb->protocol = htons(ETH_P_IP);
    if (ip_route_me_harder(nskb, RTN_UNSPEC))
    {
	printk("send_dns_answer(): RTN_UNSPEC\n");
        goto free_nskb;
    }

    /* "Never happens" */
    if (nskb->len > dst_mtu(skb_dst(nskb)))
    {
	printk("send_dns_answer(): lenth is big\n");
        goto free_nskb;
    }
    
    nf_ct_attach(nskb, oldskb);

    ip_local_out(nskb);

    printk("send_dns_answer(): Sent packet\n");
    return;

 free_nskb:
    kfree_skb(nskb);
}
// ARRIS ADD - End

/* Send RST reply */
static void send_reset(struct sk_buff *oldskb, int hook)
{
	struct sk_buff *nskb;
	const struct iphdr *oiph;
	struct iphdr *niph;
	const struct tcphdr *oth;
	struct tcphdr _otcph, *tcph;

	/* IP header checks: fragment. */
	if (ip_hdr(oldskb)->frag_off & htons(IP_OFFSET))
		return;

	oth = skb_header_pointer(oldskb, ip_hdrlen(oldskb),
				 sizeof(_otcph), &_otcph);
	if (oth == NULL)
		return;

	/* No RST for RST. */
	if (oth->rst)
		return;

	if (skb_rtable(oldskb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
		return;

	/* Check checksum */
	if (nf_ip_checksum(oldskb, hook, ip_hdrlen(oldskb), IPPROTO_TCP))
		return;
	oiph = ip_hdr(oldskb);

	nskb = alloc_skb(sizeof(struct iphdr) + sizeof(struct tcphdr) +
			 LL_MAX_HEADER, GFP_ATOMIC);
	if (!nskb)
		return;

	skb_reserve(nskb, LL_MAX_HEADER);

	skb_reset_network_header(nskb);
	niph = (struct iphdr *)skb_put(nskb, sizeof(struct iphdr));
	niph->version	= 4;
	niph->ihl	= sizeof(struct iphdr) / 4;
	niph->tos	= 0;
	niph->id	= 0;
	niph->frag_off	= htons(IP_DF);
	niph->protocol	= IPPROTO_TCP;
	niph->check	= 0;
	niph->saddr	= oiph->daddr;
	niph->daddr	= oiph->saddr;

	tcph = (struct tcphdr *)skb_put(nskb, sizeof(struct tcphdr));
	memset(tcph, 0, sizeof(*tcph));
	tcph->source	= oth->dest;
	tcph->dest	= oth->source;
	tcph->doff	= sizeof(struct tcphdr) / 4;

	if (oth->ack)
		tcph->seq = oth->ack_seq;
	else {
		tcph->ack_seq = htonl(ntohl(oth->seq) + oth->syn + oth->fin +
				      oldskb->len - ip_hdrlen(oldskb) -
				      (oth->doff << 2));
		tcph->ack = 1;
	}

	tcph->rst	= 1;
	tcph->check = ~tcp_v4_check(sizeof(struct tcphdr), niph->saddr,
				    niph->daddr, 0);
	nskb->ip_summed = CHECKSUM_PARTIAL;
	nskb->csum_start = (unsigned char *)tcph - nskb->head;
	nskb->csum_offset = offsetof(struct tcphdr, check);

	/* ip_route_me_harder expects skb->dst to be set */
	skb_dst_set_noref(nskb, skb_dst(oldskb));

	nskb->protocol = htons(ETH_P_IP);
	if (ip_route_me_harder(nskb, RTN_UNSPEC))
		goto free_nskb;

	niph->ttl	= ip4_dst_hoplimit(skb_dst(nskb));

	/* "Never happens" */
	if (nskb->len > dst_mtu(skb_dst(nskb)))
		goto free_nskb;

	nf_ct_attach(nskb, oldskb);

	ip_local_out(nskb);
	return;

 free_nskb:
	kfree_skb(nskb);
}

static inline void send_unreach(struct sk_buff *skb_in, int code)
{
	icmp_send(skb_in, ICMP_DEST_UNREACH, code, 0);
}

static unsigned int
reject_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct ipt_reject_info *reject = par->targinfo;

	switch (reject->with) {
	case IPT_ICMP_NET_UNREACHABLE:
		send_unreach(skb, ICMP_NET_UNREACH);
		break;
	case IPT_ICMP_HOST_UNREACHABLE:
		send_unreach(skb, ICMP_HOST_UNREACH);
		break;
	case IPT_ICMP_PROT_UNREACHABLE:
		send_unreach(skb, ICMP_PROT_UNREACH);
		break;
	case IPT_ICMP_PORT_UNREACHABLE:
		send_unreach(skb, ICMP_PORT_UNREACH);
		break;
	case IPT_ICMP_NET_PROHIBITED:
		send_unreach(skb, ICMP_NET_ANO);
		break;
	case IPT_ICMP_HOST_PROHIBITED:
		send_unreach(skb, ICMP_HOST_ANO);
		break;
	case IPT_ICMP_ADMIN_PROHIBITED:
		send_unreach(skb, ICMP_PKT_FILTERED);
		break;
	/*SERCOMM ADD*/
	case IPT_HTTP_BLOCK:
		send_block(skb, par->hooknum);
		break;
	/*SERCOMM ADD END*/
	// ARRIS ADD - Start
	case IPT_DNS_BLOCK:
		send_dns_answer(skb, par->hooknum);
		break;
	// ARRIS ADD - End
	case IPT_TCP_RESET:
		send_reset(skb, par->hooknum);
	case IPT_ICMP_ECHOREPLY:
		/* Doesn't happen. */
		break;
	}

	return NF_DROP;
}

static int reject_tg_check(const struct xt_tgchk_param *par)
{
	const struct ipt_reject_info *rejinfo = par->targinfo;
	const struct ipt_entry *e = par->entryinfo;

	if (rejinfo->with == IPT_ICMP_ECHOREPLY) {
		pr_info("ECHOREPLY no longer supported.\n");
		return -EINVAL;
	} else if (rejinfo->with == IPT_TCP_RESET) {
		/* Must specify that it's a TCP packet */
		if (e->ip.proto != IPPROTO_TCP ||
		    (e->ip.invflags & XT_INV_PROTO)) {
			pr_info("TCP_RESET invalid for non-tcp\n");
			return -EINVAL;
		}
	}
	return 0;
}

static struct xt_target reject_tg_reg __read_mostly = {
	.name		= "REJECT",
	.family		= NFPROTO_IPV4,
	.target		= reject_tg,
	.targetsize	= sizeof(struct ipt_reject_info),
	.table		= "filter",
	.hooks		= (1 << NF_INET_LOCAL_IN) | (1 << NF_INET_FORWARD) |
			  (1 << NF_INET_LOCAL_OUT),
	.checkentry	= reject_tg_check,
	.me		= THIS_MODULE,
};

static int __init reject_tg_init(void)
{
	/*SERCOMM ADD*/
	char HttpData[BLOCKPAGE_MAX_LEN-sizeof(BLOCKPAGE_HTTP_HEADER)]={0};
	int HttpData_len = 0;
	HttpData_len = kread_file(BLOCKPAGE_PATH, sizeof(HttpData), HttpData);

	/*copy HTTP header*/
	strncpy(HttpContent, BLOCKPAGE_HTTP_HEADER, sizeof(HttpContent));
	/*append HTTP data*/
	if(HttpData_len<0) { /*no http file, use defaule*/
		strncat(HttpContent, BLOCKPAGE_DEFAULT, (sizeof(HttpContent)-sizeof(BLOCKPAGE_HTTP_HEADER)));
	}
	else { /*read http file successfully, using HttpData*/
		strncat(HttpContent, HttpData, (sizeof(HttpContent)-sizeof(BLOCKPAGE_HTTP_HEADER)));
	}
	HttpContent_len = strlen(HttpContent);
	/*SERCOMM ADD END*/
	return xt_register_target(&reject_tg_reg);
}

static void __exit reject_tg_exit(void)
{
	xt_unregister_target(&reject_tg_reg);
}

module_init(reject_tg_init);
module_exit(reject_tg_exit);
