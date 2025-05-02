#define _LINUX_IN_H
#include <linux/types.h>
#include <netinet/ip.h>

static inline uint16_t ip_data_len(const struct ip *ip)
{
	return ntohs(ip->ip_len) - (ip->ip_hl << 2);
}

static inline void ip_set_len(struct ip *ip, int16_t len)
{
	ip->ip_len = htons(len);
}

