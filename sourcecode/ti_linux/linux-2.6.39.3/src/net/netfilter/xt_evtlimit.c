/* (C) 2015 ARRIS Group, Inc.
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/types.h>
#include <linux/fs.h>

#include <linux/if_arp.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <linux/icmpv6.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <linux/string.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter/xt_evtlimit.h>

//MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arris");
MODULE_DESCRIPTION("Xtables: rate-evtlimit match");
MODULE_ALIAS("ipt_evtlimit");
MODULE_ALIAS("ip6t_evtlimit");


static DEFINE_SPINLOCK(evtlimit_lock);

int (*arris_fw_log_read)(char* page, char** start, off_t offset, int count, int* eof, void* data) = NULL;
extern int (*daylightsaving_hook)(void);
EXPORT_SYMBOL(arris_fw_log_read);

#define MAX_EVTLIMIT_ENTRY             (8)    // number of max event type
#define EVENTLIMIT_CHECK_INTERVAL      (360)  // 6 minutes
#define EVENTLIMIT_CHECK_INTERVAL_2    (180)  // 3 minutes
#define EVENTLIMIT_CHECK_INTERVAL_3    (60)   // 1 minute

#define EVENTLIMIT_MASK_VALID_BIT             (0x01)  /* content valid or not */
#define EVENTLIMIT_MASK_ACTIVE_BIT            (0x02)  /* record active or not */
#define EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT  (0x04) /* packet truncated or not */
#define EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT (0x08) /* packet incomplete or not */
#define EVENTLIMIT_MASK_PACKET_L2_VALID_BIT   (0x10) /* IP layer 2 */
#define EVENTLIMIT_MASK_PACKET_L3_VALID_BIT   (0x20) /* IP layer 3 */
#define EVENTLIMIT_MASK_PACKET_L4_VALID_BIT   (0x40) /* IP layer 4 */



#define EVENTLIMIT_MASK_TEST(_entry, _mask) ((_entry)->mask & (_mask))
#define EVENTLIMIT_MASK_SET(_entry, _mask) {(_entry)->mask |= (_mask);}
#define EVENTLIMIT_MASK_CLEAR(_entry, _mask) {(_entry)->mask &= ~(_mask);}

#undef abs
#define abs(_v) ((_v) < 0 ? -(_v) : (_v))


typedef struct xt_evtlimit_priv
{
    u_int32_t count;
    u_int16_t count2;
    u_int16_t max_count;
    ktime_t   ktstamp;
    u_int16_t mask;
    u_int16_t refcount;
    
    char devname_in[IFNAMSIZ];
    char devname_out[IFNAMSIZ];
    struct ethhdr eth_hdr;
    u_int32_t len;
    /* use struct instead of union to minimize race condition */
    struct {
        struct {
            struct iphdr iph;
            struct tcphdr tcp_hdr;
        }v4;
        struct {
            struct ipv6hdr ip6h;
            u_int8_t fragment;
            u_int8_t nexthdr;
            struct tcphdr tcp_hdr;
        }v6;
    }ip;    
}evtlimit_entry;
struct xtm {
    u_int16_t year;     /* 1970-? */
	u_int8_t month;    /* (1-12) */
	u_int8_t monthday; /* (1-31) */
	u_int8_t weekday;  /* (1-7) */
	u_int8_t hour;     /* (0-23) */
	u_int8_t minute;   /* (0-59) */
	u_int8_t second;   /* (0-59) */
	unsigned int dse;
};

static evtlimit_entry evtlimit_entries[MAX_EVTLIMIT_ENTRY];
static struct timer_list daily_check_timer;
#define TYPE_2_ENTRY(__type) (evtlimit_entries[(__type)])

extern struct timezone sys_tz; /* ouch */

static const u_int16_t days_since_year[] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
};

static const u_int16_t days_since_leapyear[] = {
	0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335,
};

/*
 * Since time progresses forward, it is best to organize this array in reverse,
 * to minimize lookup time.
 */
enum {
	DSE_FIRST = 2039,
};
static const u_int16_t days_since_epoch[] = {
	/* 2039 - 2030 */
	25202, 24837, 24472, 24106, 23741, 23376, 23011, 22645, 22280, 21915,
	/* 2029 - 2020 */
	21550, 21184, 20819, 20454, 20089, 19723, 19358, 18993, 18628, 18262,
	/* 2019 - 2010 */
	17897, 17532, 17167, 16801, 16436, 16071, 15706, 15340, 14975, 14610,
	/* 2009 - 2000 */
	14245, 13879, 13514, 13149, 12784, 12418, 12053, 11688, 11323, 10957,
	/* 1999 - 1990 */
	10592, 10227, 9862, 9496, 9131, 8766, 8401, 8035, 7670, 7305,
	/* 1989 - 1980 */
	6940, 6574, 6209, 5844, 5479, 5113, 4748, 4383, 4018, 3652,
	/* 1979 - 1970 */
	3287, 2922, 2557, 2191, 1826, 1461, 1096, 730, 365, 0,
};

static inline bool is_leap(unsigned int y)
{
	return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

static inline unsigned int localtime_1(struct xtm *r, time_t time)
{
	unsigned int v, w;

    if (daylightsaving_hook)
    {
        int tz_minuteswest = 0;
        tz_minuteswest = daylightsaving_hook();
    	time -= 60 * tz_minuteswest;
    }
    else
    {
		time -= 60 * sys_tz.tz_minuteswest;
    }

	/* Each day has 86400s, so finding the hour/minute is actually easy. */
	v         = time % 86400;
	r->second = v % 60;
	w         = v / 60;
	r->minute = w % 60;
	r->hour   = w / 60;
	return v;
}

static inline void localtime_2(struct xtm *r, time_t time)
{
	/*
	 * Here comes the rest (weekday, monthday). First, divide the SSTE
	 * by seconds-per-day to get the number of _days_ since the epoch.
	 */
	r->dse = time / 86400;

	/*
	 * 1970-01-01 (w=0) was a Thursday (4).
	 * -1 and +1 map Sunday properly onto 7.
	 */
	r->weekday = (4 + r->dse - 1) % 7 + 1;
}

static void localtime_3(struct xtm *r, time_t time)
{
	unsigned int year, i, w = r->dse;

	/*
	 * In each year, a certain number of days-since-the-epoch have passed.
	 * Find the year that is closest to said days.
	 *
	 * Consider, for example, w=21612 (2029-03-04). Loop will abort on
	 * dse[i] <= w, which happens when dse[i] == 21550. This implies
	 * year == 2009. w will then be 62.
	 */
	for (i = 0, year = DSE_FIRST; days_since_epoch[i] > w;
	    ++i, --year)
		/* just loop */;

	w -= days_since_epoch[i];

	/*
	 * By now we have the current year, and the day of the year.
	 * r->yearday = w;
	 *
	 * On to finding the month (like above). In each month, a certain
	 * number of days-since-New Year have passed, and find the closest
	 * one.
	 *
	 * Consider w=62 (in a non-leap year). Loop will abort on
	 * dsy[i] < w, which happens when dsy[i] == 31+28 (i == 2).
	 * Concludes i == 2, i.e. 3rd month => March.
	 *
	 * (A different approach to use would be to subtract a monthlength
	 * from w repeatedly while counting.)
	 */
	if (is_leap(year)) {
		/* use days_since_leapyear[] in a leap year */
		for (i = ARRAY_SIZE(days_since_leapyear) - 1;
		    i > 0 && days_since_leapyear[i] > w; --i)
			/* just loop */;
		r->monthday = w - days_since_leapyear[i] + 1;
	} else {
		for (i = ARRAY_SIZE(days_since_year) - 1;
		    i > 0 && days_since_year[i] > w; --i)
			/* just loop */;
		r->monthday = w - days_since_year[i] + 1;
	}

	r->month    = i + 1;
    r->year     = year;
}


static int log_event_packet_v4(char *buf, int len, evtlimit_entry *e)
{
    int used = 0;
    struct iphdr *ih;

    if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT))
    {
        used += snprintf(buf + used, len - used, "TRUNCATED");
        return used;
    }

    if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L3_VALID_BIT))
    {
        ih = &e->ip.v4.iph;
        used += snprintf(buf + used, len - used, "SRC=%pI4 DST=%pI4 ", &ih->saddr, &ih->daddr);
        used += snprintf(buf + used, len - used, "LEN=%u TOS=0x%02X PREC=0x%02X TTL=%u ID=%u ",
            ntohs(ih->tot_len), ih->tos & IPTOS_TOS_MASK,  ih->tos & IPTOS_PREC_MASK, 
            ih->ttl, ntohs(ih->id));

        if (ntohs(ih->frag_off) & IP_CE)
            used += snprintf(buf+ used, len - used, "CE ");
        if (ntohs(ih->frag_off) & IP_DF)
            used += snprintf(buf + used, len - used, "DF ");
        if (ntohs(ih->frag_off) & IP_MF)
            used += snprintf(buf + used, len - used, "MF ");
        if (ntohs(ih->frag_off) & IP_OFFSET)
            used += snprintf(buf + used, len - used, "FRAG:%u ", ntohs(ih->frag_off) & IP_OFFSET);

        switch (ih->protocol){
            case IPPROTO_TCP:{
                struct tcphdr *th;
                used += snprintf(buf+used, len-used, "PROTO=TCP ");
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE [%u bytes] ", e->len - ih->ihl * 4);
                    break;
                }
                if (!EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT)) break;
                th = &e->ip.v4.tcp_hdr;
                used += snprintf(buf+used, len-used, "SPT=%u DPT=%u ", ntohs(th->source), ntohs(th->dest));
                used += snprintf(buf+used, len-used, "WINDOW=%u ", ntohs(th->window));
                /* no RES */
                if (th->cwr)
                    used += snprintf(buf+used, len-used, "CWR ");
                if (th->ece)
                    used += snprintf(buf+used, len-used, "ECE ");
                if (th->urg)
                    used += snprintf(buf+used, len-used, "URG ");
                if (th->ack)
                    used += snprintf(buf+used, len-used, "ACK ");
                if (th->psh)
                    used += snprintf(buf+used, len-used, "PSH ");
                if (th->rst)
                    used += snprintf(buf+used, len-used, "RST ");
                if (th->syn)
                    used += snprintf(buf+used, len-used, "SYN ");
                if (th->fin)
                    used += snprintf(buf+used, len-used, "FIN ");
                used += snprintf(buf+used, len-used, "URGP=%u ", ntohs(th->urg_ptr));
                break;
            }
            case IPPROTO_UDP:
            case IPPROTO_UDPLITE:{
                struct udphdr *uh = (struct udphdr *)&e->ip.v4.tcp_hdr;
                if (ih->protocol == IPPROTO_UDP)
                    used += snprintf(buf+used, len-used, "PROTO=UDP ");
                else
                    used += snprintf(buf+used, len-used, "PROTO=UDPLITE ");
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE [%u bytes] ",
                        e->len - ih->ihl * 4);
                    break;
                }
                used += snprintf(buf+used, len-used, "SPT=%u DPT=%u LEN=%u ", ntohs(uh->source),
                    ntohs(uh->dest), ntohs(uh->len));
                break;
            }
            case IPPROTO_ICMP:{
                struct icmphdr *ich = (struct icmphdr*)&e->ip.v4.tcp_hdr;
                used += snprintf(buf+used, len-used, "PROTO=ICMP ");
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE [%u bytes] ", e->len - ih->ihl * 4);
                    break;
                }
                if (!EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT)) break;
                used += snprintf(buf+used, len-used, "TYPE=%u CODE=%u ", ich->type, ich->code);

                switch (ich->type){
                    case ICMP_ECHOREPLY:
                    case ICMP_ECHO:
                        used += snprintf(buf+used, len-used, "ID=%u SEQ=%u ", ntohs(ich->un.echo.id), ntohs(ich->un.echo.sequence));
                        break;
                    case ICMP_PARAMETERPROB:
                        used += snprintf(buf+used, len-used, "PARAMETER=%u ", ntohs(ich->un.gateway) >> 24);
                        break;
                    case ICMP_REDIRECT:
                        used += snprintf(buf+used, len-used, "GATEWAY=%pI4 ", &ich->un.gateway);
                    case ICMP_DEST_UNREACH:
                    case ICMP_SOURCE_QUENCH:
                    case ICMP_TIME_EXCEEDED:
                        if (ich->type == ICMP_DEST_UNREACH &&
                            ich->code == ICMP_FRAG_NEEDED)
                            used += snprintf(buf+used, len-used, "MTU=%u ", ntohs(ich->un.frag.mtu));
                        break;
                }
                break;
            }
            default:
                used += snprintf(buf+used, len-used, "PROTO=%u ", ih->protocol);
                break;
        }
    }
    return used;
}

static int log_event_packet_v6(char *buf, int len, evtlimit_entry *e)
{
    int used = 0;
    struct ipv6hdr *ih;

    if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT) &&
        !EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L3_VALID_BIT))
    {
        used += snprintf(buf + used, len - used, "TRUNCATED");
        return used;
    }

    if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L3_VALID_BIT))
    {
        ih = &e->ip.v6.ip6h;
        used += snprintf(buf + used, len - used, "SRC=%pI6c DST=%pI6c ", &ih->saddr, &ih->daddr);
        used += snprintf(buf + used, len - used, "LEN=%Zu TC=%u HOPLIMIT=%u FLOWLBL=%u ",
            ntohs(ih->payload_len) + sizeof(struct ipv6hdr),
            (ntohl(*(__be32*)ih) & 0x0ff00000) >> 20,
            ih->hop_limit,
            (ntohl(*(__be32*)ih) & 0x000fffff));
        if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT))
        {
            used += snprintf(buf + used, len - used, "TRUNCATED");
            return used;
        }
        
        switch (e->ip.v6.nexthdr){
            case IPPROTO_TCP:{
                struct tcphdr *th;
                used += snprintf(buf+used, len-used, "PROTO=TCP ");
                if (e->ip.v6.fragment)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE ");
                    break;
                }
                if (!EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT)) break;
                th = &e->ip.v6.tcp_hdr;
                used += snprintf(buf+used, len-used, "SPT=%u DPT=%u ", ntohs(th->source), ntohs(th->dest));
                used += snprintf(buf+used, len-used, "WINDOW=%u ", ntohs(th->window));
                /* no RES */
                if (th->cwr)
                    used += snprintf(buf+used, len-used, "CWR ");
                if (th->ece)
                    used += snprintf(buf+used, len-used, "ECE ");
                if (th->urg)
                    used += snprintf(buf+used, len-used, "URG ");
                if (th->ack)
                    used += snprintf(buf+used, len-used, "ACK ");
                if (th->psh)
                    used += snprintf(buf+used, len-used, "PSH ");
                if (th->rst)
                    used += snprintf(buf+used, len-used, "RST ");
                if (th->syn)
                    used += snprintf(buf+used, len-used, "SYN ");
                if (th->fin)
                    used += snprintf(buf+used, len-used, "FIN ");
                used += snprintf(buf+used, len-used, "URGP=%u ", ntohs(th->urg_ptr));
                break;
            }
            case IPPROTO_UDP:
            case IPPROTO_UDPLITE:{
                struct udphdr *uh = (struct udphdr *)&e->ip.v6.tcp_hdr;
                if (e->ip.v6.nexthdr == IPPROTO_UDP)
                    used += snprintf(buf+used, len-used, "PROTO=UDP ");
                else
                    used += snprintf(buf+used, len-used, "PROTO=UDPLITE ");
                if (e->ip.v6.fragment)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE ");
                    break;
                }
                used += snprintf(buf+used, len-used, "SPT=%u DPT=%u LEN=%u ", ntohs(uh->source),
                    ntohs(uh->dest), ntohs(uh->len));
                break;
            }
            case IPPROTO_ICMPV6:{
                struct icmp6hdr *ich = (struct icmp6hdr*)&e->ip.v6.tcp_hdr;
                used += snprintf(buf+used, len-used, "PROTO=ICMPv6 ");
                if (e->ip.v6.fragment)
                    break;
                if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT))
                {
                    used += snprintf(buf+used, len-used, "INCOMPLETE ");
                    break;
                }
                if (!EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT)) break;
                used += snprintf(buf+used, len-used, "TYPE=%u CODE=%u ", ich->icmp6_type, ich->icmp6_code);

                switch (ich->icmp6_type){
                    case ICMPV6_ECHO_REQUEST:
                    case ICMPV6_ECHO_REPLY:
                        used += snprintf(buf+used, len-used, "ID=%u SEQ=%u ", ntohs(ich->icmp6_identifier),
                            ntohs(ich->icmp6_sequence));
                        break;
                    case ICMPV6_MGM_QUERY:
                    case ICMPV6_MGM_REPORT:
                    case ICMPV6_MGM_REDUCTION:
                        break;
                    case ICMPV6_PARAMPROB:
                        used += snprintf(buf+used, len-used, "POINTER=%08x ", ntohs(ich->icmp6_pointer));
                    case ICMPV6_DEST_UNREACH:
                    case ICMPV6_PKT_TOOBIG:
                    case ICMPV6_TIME_EXCEED:
                        if (ich->icmp6_type == ICMPV6_PKT_TOOBIG)
                            used += snprintf(buf+used, len-used, "MTU=%u ", ntohs(ich->icmp6_mtu));
                        break;
                        
                }
                break;
            }
            default:
                used += snprintf(buf+used, len-used, "PROTO=%u ", e->ip.v6.nexthdr);
                break;
        }
    }
    return used;
}

static int log_event_packet(char *buf, int len, evtlimit_entry *e)
{
    int used = 0;

    /* IN/OUT dev */
    used += snprintf(buf + used, len - used, "IN=%s OUT=%s ", e->devname_in, e->devname_out);

    /* MAC */
    if (EVENTLIMIT_MASK_TEST(e, EVENTLIMIT_MASK_PACKET_L2_VALID_BIT))
    {
        used += snprintf(buf + used, len - used, "MACSRC=%pM MACDST=%pM MACPROTO=%04x ",
            e->eth_hdr.h_source, e->eth_hdr.h_dest, ntohs(e->eth_hdr.h_proto));
    }
    else
    {
        used += snprintf(buf + used, len - used, "MAC= ");
    }

    if (htons(e->eth_hdr.h_proto) == ETH_P_IP)
    {
        used += log_event_packet_v4(buf + used, len - used, e);
    }
    else if (htons(e->eth_hdr.h_proto) == ETH_P_IPV6)
    {
        used += log_event_packet_v6(buf + used, len - used, e);
    }
    else
    {
    }
    
    return used;    
}

static void record_packet(const struct sk_buff *skb, struct xt_evtlimit_priv *priv, struct xt_action_param *par)
{
    struct net_device *dev = skb->dev;
    /* in/out dev name */
    priv->devname_in[0] = 0;
    priv->devname_out[0] = 0;
    if (par->in && par->in->name[0]) strcpy(priv->devname_in, par->in->name);
    if (par->out && par->out->name[0]) strcpy(priv->devname_out, par->out->name);

    EVENTLIMIT_MASK_CLEAR(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT | EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT
            | EVENTLIMIT_MASK_PACKET_L2_VALID_BIT | EVENTLIMIT_MASK_PACKET_L3_VALID_BIT
            | EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_VALID_BIT);
    
    /* mac header */
    if (skb->mac_header != skb->network_header)
    {
        if (dev->type == ARPHRD_ETHER)
        {
            memcpy(&priv->eth_hdr, eth_hdr(skb), sizeof(priv->eth_hdr));
            EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L2_VALID_BIT);
        }
        else
        {
            if (dev->hard_header_len == sizeof(priv->eth_hdr))
            {
                memcpy(&priv->eth_hdr, skb_mac_header(skb), sizeof(priv->eth_hdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L2_VALID_BIT);
            }
        }
    }

    priv->len = skb->len;
    
    if (par->family == NFPROTO_IPV4)
    {
        struct iphdr _iph;
        const struct iphdr *ih;
        priv->eth_hdr.h_proto == htons(ETH_P_IP);
        if (!(ih = skb_header_pointer(skb, 0, sizeof(_iph), &_iph)))
        {
            EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT);
            return;
        }

        memcpy(&priv->ip.v4.iph, ih, sizeof(_iph));
        EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L3_VALID_BIT);

        /* no IP OPT */

        switch (ih->protocol)
        {
            case IPPROTO_TCP:{
                struct tcphdr _tcph;
                const struct tcphdr *th;
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (!(th = skb_header_pointer(skb, ih->ihl * 4, sizeof(_tcph), &_tcph)))
                {
                    /* incomplete */
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    break;
                }
                memcpy(&priv->ip.v4.tcp_hdr, th,  sizeof(struct tcphdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            case IPPROTO_UDP:
            case IPPROTO_UDPLITE:{
                struct udphdr _udph;
                const struct udphdr *uh;
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (!(uh = skb_header_pointer(skb, ih->ihl * 4, sizeof(_udph), &_udph)))
                {
                    /* incomplete */
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    break;
                }

                memcpy(&priv->ip.v4.tcp_hdr, uh, sizeof(struct udphdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            case IPPROTO_ICMP:{
                struct icmphdr _icmph;
                const struct icmphdr *ich;
                if (ntohs(ih->frag_off) & IP_OFFSET)
                    break;
                if (!(ich = skb_header_pointer(skb, ih->ihl * 4, sizeof(_icmph), &_icmph)))
                {
                    /* incomplete */
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    break;
                }
                memcpy(&priv->ip.v4.tcp_hdr, ich, sizeof(struct icmphdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            default:
                break;
        }
    }
    else if (par->family == NFPROTO_IPV6)
    {
        unsigned int ptr, hdrlen = 0;
        int fragment;
        u_int8_t currenthdr;
        struct ipv6hdr _ip6h;
        const struct ipv6hdr *ih;
        unsigned int ip6hoff = skb_network_offset(skb);
        priv->eth_hdr.h_proto == htons(ETH_P_IPV6);
        if (!(ih = skb_header_pointer(skb, ip6hoff, sizeof(_ip6h), &_ip6h)))
        {
            EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT);
            return;
        }

        /* ipv6 header */
        memcpy(&priv->ip.v6.ip6h, ih, sizeof(struct ipv6hdr));
        EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L3_VALID_BIT);
        priv->ip.v6.fragment = 0;
        priv->ip.v6.nexthdr = 0;
        
        fragment = 0;
        ptr = ip6hoff + sizeof(struct ipv6hdr);
        currenthdr = ih->nexthdr;
        while (currenthdr != NEXTHDR_NONE && ip6t_ext_hdr(currenthdr)){
            struct ipv6_opt_hdr _hdr;
            const struct ipv6_opt_hdr *hp;
            if (!(hp = skb_header_pointer(skb, ptr, sizeof(_hdr), &_hdr)))
            {
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT);
                return;
            }

            switch (currenthdr){
                case IPPROTO_FRAGMENT:{
                    struct frag_hdr _fhdr;
                    const struct frag_hdr *fh;
                    if (!(fh = skb_header_pointer(skb, ptr, sizeof(_fhdr), &_fhdr)))
                    {
                        EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_TRUNCATED_BIT);
                        return;
                    }
                    if (ntohs(fh->frag_off) & 0xFFF8) fragment = 1;
                    hdrlen = 8;
                    break;
                }
                case IPPROTO_DSTOPTS:
                case IPPROTO_ROUTING:
		        case IPPROTO_HOPOPTS:
                {
                    if (fragment) return;
                    hdrlen = ipv6_optlen(hp);
                    break;
		        }
                case IPPROTO_AH:
                    hdrlen = (hp->hdrlen + 2) << 2;
                    break;
                case IPPROTO_ESP:
                default:
                    return;
            }

            currenthdr = hp->nexthdr;
            ptr += hdrlen;
        }
        
        priv->ip.v6.nexthdr = currenthdr;

        switch (currenthdr){
            case IPPROTO_TCP:{
                struct tcphdr _tcph;
                const struct tcphdr *th;
                if (fragment)
                {
                    priv->ip.v6.fragment = 1;
                    break;
                }
                if (!(th = skb_header_pointer(skb, ptr, sizeof(_tcph), &_tcph)))
                {
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    return;
                }
                /* tcp header */
                memcpy(&priv->ip.v6.tcp_hdr, th, sizeof(struct tcphdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            case IPPROTO_UDP:
            case IPPROTO_UDPLITE:{
                struct udphdr _udph;
		        const struct udphdr *uh;
                if (fragment)
                {
                    priv->ip.v6.fragment = 1;
                    break;
                }
                if (!(uh = skb_header_pointer(skb, ptr, sizeof(_udph), &_udph)))
                {
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    return;
                }

                /* udp header */
                memcpy(&priv->ip.v6.tcp_hdr, uh, sizeof(struct udphdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            case IPPROTO_ICMPV6:{
                struct icmp6hdr _icmp6h;
		        const struct icmp6hdr *ic;
                if (fragment)
                {
                    priv->ip.v6.fragment = 1;
                    break;
                }
                if (!(ic = skb_header_pointer(skb, ptr, sizeof(_icmp6h), &_icmp6h)))
                {
                    EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_INCOMPLETE_BIT);
                    return;
                }

                /* icmpv6 header */
                memcpy(&priv->ip.v6.tcp_hdr, ic, sizeof(struct icmp6hdr));
                EVENTLIMIT_MASK_SET(priv, EVENTLIMIT_MASK_PACKET_L4_VALID_BIT);
                break;
            }
            default:                
                break;
        }
    }    
}


static int arris_proc_read_fw_log(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    int len=0;
    int i;
    evtlimit_entry *entry;
    s64 stamp;
    struct xtm current_time;
    char buf[1024];
    
    for (i = 0; i < ARRAY_SIZE(evtlimit_entries); ++i)
    {
        entry = &evtlimit_entries[i];
        if (EVENTLIMIT_MASK_TEST(entry, EVENTLIMIT_MASK_VALID_BIT))
        {
            stamp = ktime_to_ns(entry->ktstamp);
            stamp = div_s64(stamp, NSEC_PER_SEC);
            /* change to local time */
            localtime_1(&current_time, stamp);
            localtime_2(&current_time, stamp);
            localtime_3(&current_time, stamp);
            log_event_packet(buf, sizeof(buf), entry);
            len += sprintf(page+len, "%d %u %04d-%02d-%02d %02d:%02d:%02d %s\n", i, entry->count, current_time.year, 
                current_time.month, current_time.monthday, current_time.hour, current_time.minute, current_time.second, buf);
        }
    }
    
    *eof = 1;
    return len;
}

static void midnight_check(unsigned long data)
{
    ktime_t kstamp;
    s64 stamp;
    struct xtm now;
    u_int32_t interval;
    int32_t   left;
    
    kstamp = ktime_get_real();
    stamp = ktime_to_ns(kstamp);
    stamp = div_s64(stamp, NSEC_PER_SEC);

    /* change to local time */
    localtime_1(&now, stamp);

    /* how many seconds left to mid-night */
    left = (24 - (!now.hour ? 24 : now.hour)) * 60 - now.minute;
    left = (left >= 0 ? -now.second : now.second) + abs(left) * 60;
    interval = EVENTLIMIT_CHECK_INTERVAL;

    /* Don't consider the timer enters EVENTLIMIT_CHECK_INTERVAL_3 to midnight without entering
       EVENTLIMIT_CHECK_INTERVAL_2 is ok. The reason is as following:    
       If the timer is setup near midnight (7 minutes to midnight), which means that the kernel 
       is just boot up near midnight, don't reset the counter and send email makes sense.
    */

    if (left < EVENTLIMIT_CHECK_INTERVAL_3)/* trigger mid-night */
    {
        evtlimit_entry *entry;
        int i;
        printk(KERN_DEBUG"<midnight_check> midnight triggered at %02d:%02d:%02d, left:%d, next inteval:%u\n", now.hour,
                now.minute, now.second, left, interval);
        for (i = 0; i < ARRAY_SIZE(evtlimit_entries); ++i)
        {
            entry = &evtlimit_entries[i];
            if (EVENTLIMIT_MASK_TEST(entry, EVENTLIMIT_MASK_ACTIVE_BIT) && entry->count >= entry->max_count)
            {
                /* reset counter */
                entry->count = 0;
            }
        }
    }
    else if (left <= EVENTLIMIT_CHECK_INTERVAL_2)
    {
        /* email notify */        
        interval = EVENTLIMIT_CHECK_INTERVAL_3;
        printk(KERN_NOTICE"Firewall_Event_Log_Midnight\n");
        printk(KERN_DEBUG"<midnight_check> midnight email triggered at %02d:%02d:%02d, left:%d, next interval:%u\n", now.hour,
                now.minute, now.second, left, interval);
    }
    else if (left <= (EVENTLIMIT_CHECK_INTERVAL + EVENTLIMIT_CHECK_INTERVAL))
    {
        if (now.hour)
            interval = left - EVENTLIMIT_CHECK_INTERVAL_2 + 1;
        printk(KERN_DEBUG"<midnight_check> at %02d:%02d:%02d, left:%d, next interval:%u\n", now.hour, now.minute, now.second, left,  interval);
    }
    else
    {
        printk(KERN_DEBUG"<midnight_check> at %02d:%02d:%02d, left:%d, next interval:%u\n", now.hour, now.minute, now.second, left, interval);
    }

    mod_timer(&daily_check_timer, jiffies + interval * HZ);
}

static bool evtlimit_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_evtrateinfo *r = par->matchinfo;
	struct xt_evtlimit_priv *priv = r->master;
	bool ret = false;

    if (skb->tstamp.tv64 == 0)
        __net_timestamp((struct sk_buff*)skb);    

	spin_lock_bh(&evtlimit_lock);
    priv->count += 1;
    priv->ktstamp = skb->tstamp;
    
    if (priv->count <= r->threshold)
    {
        ret = true;
    }
    else
    {
        /* ignore at constant percent */  
        if (priv->count2 < (r->count2 - r->count1))
        {
            ret = false;
            priv->count2 += 1;
        }
        else
        {
            ret = true;
            if (priv->count2 >= r->count2 -1)
            {
                priv->count2 = 0;
            }
            else 
            {
                priv->count2 += 1;
            }
        }      
    }
    record_packet(skb, priv, par);
	spin_unlock_bh(&evtlimit_lock);
    
	return ret;
}

static int evtlimit_mt_check(const struct xt_mtchk_param *par)
{
	struct xt_evtrateinfo *r = par->matchinfo;

	/* Check for overflow. */
    if (r->type >= MAX_EVTLIMIT_ENTRY)
    {
        pr_info("Overflow, try lower type: %u\n", r->type);
        return -ERANGE;
    }

    if (r->percent > 100)
    {
        pr_info("Overflow, try lower percent: %u\n", r->percent);
        return -ERANGE;
    }

    if (r->count1 > r->count2)
    {
        pr_info("Overflow, invalid rates: %u/%u\n", r->count1, r->count2);
        return -ERANGE;
    }

    if (!r->max_count)
    {
        pr_info("Invalid max_count: %u\n", r->max_count);
        return -ERANGE;
    }

	r->master = &TYPE_2_ENTRY(r->type);
    r->master->refcount += 1;
    if (!EVENTLIMIT_MASK_TEST(r->master, EVENTLIMIT_MASK_ACTIVE_BIT))
    {
        r->master->count = 0;
        r->master->max_count = r->max_count;
    }
    EVENTLIMIT_MASK_SET(r->master, EVENTLIMIT_MASK_ACTIVE_BIT);
	return 0;
}

static void evtlimit_mt_destroy(const struct xt_mtdtor_param *par)
{
	const struct xt_evtrateinfo *r = par->matchinfo;
	struct xt_evtlimit_priv *priv = r->master;

    if (!(--priv->refcount))
    {
        EVENTLIMIT_MASK_CLEAR(priv, EVENTLIMIT_MASK_ACTIVE_BIT);
        //priv->count = 0;
        priv->count2 = 0;
    }
}

static struct xt_match evtlimit_mt_reg[] __read_mostly = {
    {
    	.name             = "evtlimit",
    	.revision         = 0,
    	.family           = NFPROTO_IPV4,
    	.match            = evtlimit_mt,
    	.checkentry       = evtlimit_mt_check,
    	.destroy          = evtlimit_mt_destroy,
    	.matchsize        = sizeof(struct xt_evtrateinfo),
#ifdef CONFIG_COMPAT
    	.compatsize       = 0,
    	.compat_from_user = NULL,
    	.compat_to_user   = NULL,
#endif
    	.me               = THIS_MODULE,
    },
    {
        .name             = "evtlimit",
        .revision         = 0,
        .family           = NFPROTO_IPV6,
        .match            = evtlimit_mt,
        .checkentry       = evtlimit_mt_check,
        .destroy          = evtlimit_mt_destroy,
        .matchsize        = sizeof(struct xt_evtrateinfo),
#ifdef CONFIG_COMPAT
        .compatsize       = 0,
        .compat_from_user = NULL,
        .compat_to_user   = NULL,
#endif
        .me               = THIS_MODULE,
    },
};

static int __init evtlimit_mt_init(void)
{
    int ret;
    setup_timer(&daily_check_timer, midnight_check, (unsigned long)0);
    ret = mod_timer(&daily_check_timer, jiffies + EVENTLIMIT_CHECK_INTERVAL*HZ);
    if (ret) printk("Error returned by mod_timer in evtlimit_mt_init\n");

    arris_fw_log_read = arris_proc_read_fw_log;
	return xt_register_matches(&evtlimit_mt_reg, 2);
}

static void __exit evtlimit_mt_exit(void)
{
    arris_fw_log_read = NULL;
    del_timer(&daily_check_timer);
	xt_unregister_match(&evtlimit_mt_reg);
}

module_init(evtlimit_mt_init);
module_exit(evtlimit_mt_exit);
